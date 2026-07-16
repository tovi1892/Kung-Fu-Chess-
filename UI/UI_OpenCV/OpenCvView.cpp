#include "OpenCvView.hpp"

#include <sstream>

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

        if (rp.cooldownMs > 0) {
            std::ostringstream ss;
            ss << static_cast<int>(rp.cooldownMs);
            frame.put_text(ss.str(), px + 4, py + 16, 0.4, cv::Scalar(0, 0, 255, 255), 1);
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
