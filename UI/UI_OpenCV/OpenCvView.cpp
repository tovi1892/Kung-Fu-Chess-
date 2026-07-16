#include "OpenCvView.hpp"

#include <algorithm>

#include "model/GameConfig.hpp"

namespace kungfu {

OpenCvView::OpenCvView(int width, int height)
    : width_(width), height_(height), windowName_("KungFuChess"),
      mapper_(width, height, GameConfig::kBoardSize, GameConfig::kBoardSize) {}

void OpenCvView::init() {
    drawBoard();

    cv::namedWindow(windowName_, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(windowName_, &OpenCvView::onMouse, this);
}

void OpenCvView::drawBoard() {
    const int margin = mapper_.margin();
    const int rows = GameConfig::kBoardSize;
    const int cols = GameConfig::kBoardSize;

    boardImg_.create(width_, height_, cv::Scalar(235, 235, 235));

    static const cv::Scalar kLightSquare(181, 217, 240);
    static const cv::Scalar kDarkSquare(99, 136, 181);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const bool isLight = (r + c) % 2 == 0;
            boardImg_.draw_rect(mapper_.cellTopLeftX(c), mapper_.cellTopLeftY(r),
                                 mapper_.cellWidth(), mapper_.cellHeight(),
                                 isLight ? kLightSquare : kDarkSquare);
        }
    }

    static const cv::Scalar kLabelColor(60, 60, 60);
    for (int c = 0; c < cols; ++c) {
        const std::string label(1, static_cast<char>('a' + c));
        const int x = mapper_.cellTopLeftX(c) + mapper_.cellWidth() / 2 - 5;
        boardImg_.put_text(label, x, margin - 12, 0.5, kLabelColor, 1);
        boardImg_.put_text(label, x, height_ - margin + 24, 0.5, kLabelColor, 1);
    }
    for (int r = 0; r < rows; ++r) {
        const std::string label = std::to_string(rows - r);
        const int y = mapper_.cellTopLeftY(r) + mapper_.cellHeight() / 2 + 5;
        boardImg_.put_text(label, margin / 2 - 5, y, 0.5, kLabelColor, 1);
        boardImg_.put_text(label, width_ - margin / 2 - 5, y, 0.5, kLabelColor, 1);
    }
}

namespace {
void drawCellOutline(Img& frame, const CoordinateMapper& mapper, int row, int col, const cv::Scalar& color) {
    frame.draw_rect_outline(mapper.cellTopLeftX(col), mapper.cellTopLeftY(row),
                             mapper.cellWidth(), mapper.cellHeight(), color, 3);
}

cv::Scalar lerpColor(const cv::Scalar& from, const cv::Scalar& to, double t) {
    return cv::Scalar(from[0] + (to[0] - from[0]) * t,
                       from[1] + (to[1] - from[1]) * t,
                       from[2] + (to[2] - from[2]) * t);
}

// A radial "charging up" meter for a piece's post-move cooldown: a dim track
// ring plus a brighter arc that sweeps clockwise from 12 o'clock as the
// cooldown elapses (empty right after landing, a full ring once selectable
// again), colored from rust to gold so it reads as "warming up" rather than
// alarming - deliberately distinct from the white selection / green
// last-move outlines so the three highlights never get visually confused.
void drawCooldownRing(Img& frame, const CoordinateMapper& mapper, int cellPx, int cellPy,
                       double remainingMs, double totalMs) {
    static const cv::Scalar kTrackColor(70, 70, 70);
    static const cv::Scalar kStartColor(20, 90, 170);    // rust - just landed, long wait ahead
    static const cv::Scalar kReadyColor(40, 210, 255);   // gold - about to become selectable

    const double fraction = std::clamp(1.0 - (remainingMs / totalMs), 0.0, 1.0);
    const int radius = std::min(mapper.cellWidth(), mapper.cellHeight()) / 2 - 4;
    const int centerX = cellPx + mapper.cellWidth() / 2;
    const int centerY = cellPy + mapper.cellHeight() / 2;

    frame.draw_arc(centerX, centerY, radius, 0.0, 360.0, kTrackColor, 3);

    const double endAngleDeg = -90.0 + fraction * 360.0;
    frame.draw_arc(centerX, centerY, radius, -90.0, endAngleDeg, lerpColor(kStartColor, kReadyColor, fraction), 4);
}
}  // namespace

void OpenCvView::render(const std::vector<RenderPiece>& pieces, const BoardHighlight& highlight) {
    Img frame = boardImg_.clone();

    static const cv::Scalar kSelectionColor(255, 255, 255);   // white - the square currently selected
    static const cv::Scalar kLastMoveColor(80, 220, 80);      // green - the last move's from/to squares

    if (highlight.hasSelection) {
        drawCellOutline(frame, mapper_, highlight.selectedRow, highlight.selectedCol, kSelectionColor);
    }
    if (highlight.hasLastMove) {
        drawCellOutline(frame, mapper_, highlight.lastMoveFromRow, highlight.lastMoveFromCol, kLastMoveColor);
        drawCellOutline(frame, mapper_, highlight.lastMoveToRow, highlight.lastMoveToCol, kLastMoveColor);
    }

    for (const auto& rp : pieces) {
        // rp.x/rp.y are already fractional (GameEngine interpolates them
        // while a piece is mid-move) - map them straight to sub-cell pixel
        // position instead of rounding to a whole cell first, or the piece
        // would visibly jump once mid-flight instead of sliding smoothly.
        const int px = static_cast<int>(mapper_.cellTopLeftXf(rp.y) + 0.5);
        const int py = static_cast<int>(mapper_.cellTopLeftYf(rp.x) + 0.5);

        const auto& animations = assets_.getAnimations(static_cast<PieceType>(rp.type),
                                                         static_cast<PlayerColor>(rp.color),
                                                         mapper_.cellWidth(), mapper_.cellHeight());
        const auto pieceState = static_cast<PieceState>(rp.state);
        const auto& sequence = animations.forState(pieceState);
        if (sequence.frames.empty()) {
            continue;
        }

        const int frameIndex = animator_.frameIndexFor(rp.id, pieceState, sequence);
        sequence.frames[frameIndex].draw_on(frame, px, py);

        if (rp.cooldownMs > 0 && rp.cooldownTotalMs > 0) {
            drawCooldownRing(frame, mapper_, px, py, rp.cooldownMs, rp.cooldownTotalMs);
        }
    }

    cv::imshow(windowName_, frame.get_mat());

    const int key = cv::waitKey(16);
    if (key == 27 || cv::getWindowProperty(windowName_, cv::WND_PROP_VISIBLE) < 1) {
        closed_ = true;
    }
}

bool OpenCvView::isOpen() const {
    return !closed_;
}

void OpenCvView::setInputHandler(IInputHandlerPtr handler) {
    inputHandler_ = std::move(handler);
}

void OpenCvView::onMouse(int event, int x, int y, int /*flags*/, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }
    auto* self = static_cast<OpenCvView*>(userdata);
    if (self && self->inputHandler_) {
        self->inputHandler_->handleClick(x, y);
    }
}

} // namespace kungfu
