#include "OpenCvView.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "model/GameConfig.hpp"

namespace kungfu {

OpenCvView::OpenCvView(int boardSize, int sidePanelWidth)
    : boardSize_(boardSize), sidePanelWidth_(sidePanelWidth),
      width_(sidePanelWidth_ * 2 + boardSize_), height_(boardSize_),
      windowName_("KungFuChess"),
      mapper_(boardSize_, boardSize_, GameConfig::kBoardSize, GameConfig::kBoardSize,
              CoordinateMapper::kDefaultMargin, sidePanelWidth_, 0) {}

void OpenCvView::init() {
    drawStaticBackground();

    cv::namedWindow(windowName_, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(windowName_, &OpenCvView::onMouse, this);
}

void OpenCvView::drawStaticBackground() {
    const int margin = mapper_.margin();
    const int rows = GameConfig::kBoardSize;
    const int cols = GameConfig::kBoardSize;

    static const cv::Scalar kPanelBg(20, 20, 20);
    boardImg_.create(width_, height_, kPanelBg);

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

    // Labels sit in the margin around the board, which is part of the same
    // dark panel background, not the light checkered squares - so they need
    // a light color to stay readable.
    static const cv::Scalar kLabelColor(215, 215, 215);
    for (int c = 0; c < cols; ++c) {
        const std::string label(1, static_cast<char>('a' + c));
        const int x = mapper_.cellTopLeftX(c) + mapper_.cellWidth() / 2 - 5;
        boardImg_.put_text(label, x, margin - 12, 0.5, kLabelColor, 1);
        boardImg_.put_text(label, x, boardSize_ - margin + 24, 0.5, kLabelColor, 1);
    }
    for (int r = 0; r < rows; ++r) {
        const std::string label = std::to_string(rows - r);
        const int y = mapper_.cellTopLeftY(r) + mapper_.cellHeight() / 2 + 5;
        boardImg_.put_text(label, sidePanelWidth_ + margin / 2 - 5, y, 0.5, kLabelColor, 1);
        boardImg_.put_text(label, sidePanelWidth_ + boardSize_ - margin / 2 - 5, y, 0.5, kLabelColor, 1);
    }

    static const cv::Scalar kDividerColor(75, 75, 75);
    boardImg_.draw_rect(sidePanelWidth_ - 1, 0, 1, height_, kDividerColor);
    boardImg_.draw_rect(sidePanelWidth_ + boardSize_, 0, 1, height_, kDividerColor);
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

// A radial "charging up" meter for any piece-unavailable timer with a known
// remaining/total duration (post-move cooldown or post-jump short rest): a
// dim track ring plus a brighter arc that sweeps clockwise from 12 o'clock
// as the wait elapses (empty right after landing, a full ring once
// selectable again), colored from rust to gold so it reads as "warming up"
// rather than alarming - deliberately distinct from the white selection /
// green last-move outlines so the highlights never get visually confused.
void drawRestRing(Img& frame, const CoordinateMapper& mapper, int cellPx, int cellPy,
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

// mm:ss.mmm, matching the reference layout's TIME column.
std::string formatElapsed(double elapsedMs) {
    const int totalMs = static_cast<int>(elapsedMs);
    const int minutes = totalMs / 60000;
    const int seconds = (totalMs / 1000) % 60;
    const int millis = totalMs % 1000;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ':'
        << std::setw(2) << seconds << '.'
        << std::setw(3) << millis;
    return oss.str();
}

// One player's side panel: name + score centered at the top, then a
// TIME/MOVE move list below. Only as many of the most recent moves as fit
// in the panel's remaining height are shown, so the latest move is always
// visible instead of the list silently overflowing off-screen.
void drawPlayerPanel(Img& frame, const PlayerPanel& panel, int panelX, int panelWidth, int panelHeight) {
    static const cv::Scalar kNameColor(60, 200, 230);
    static const cv::Scalar kScoreColor(190, 190, 190);
    static const cv::Scalar kHeaderColor(225, 225, 225);
    static const cv::Scalar kMoveColor(205, 205, 205);
    static const cv::Scalar kDividerColor(90, 90, 90);

    auto centeredX = [&](const std::string& text, double fontSize, int thickness) {
        const int textWidth = frame.text_width(text, fontSize, thickness);
        return panelX + std::max(0, (panelWidth - textWidth) / 2);
    };

    const std::string name = panel.name.empty() ? std::string("Player") : panel.name;
    frame.put_text(name, centeredX(name, 0.65, 2), 34, 0.65, kNameColor, 2);

    const std::string scoreText = "Score: " + std::to_string(panel.score);
    frame.put_text(scoreText, centeredX(scoreText, 0.45, 1), 60, 0.45, kScoreColor, 1);

    const int headerY = 95;
    const int timeColX = panelX + 18;
    const int moveColX = panelX + panelWidth / 2 + 10;
    frame.put_text("TIME", timeColX, headerY, 0.42, kHeaderColor, 1);
    frame.put_text("MOVE", moveColX, headerY, 0.42, kHeaderColor, 1);
    frame.draw_rect(panelX + 10, headerY + 8, panelWidth - 20, 1, kDividerColor);

    const int rowStartY = headerY + 28;
    const int rowHeight = 20;
    const int maxRows = std::max(0, (panelHeight - rowStartY - 12) / rowHeight);

    const int total = static_cast<int>(panel.moves.size());
    const int startIndex = std::max(0, total - maxRows);
    int y = rowStartY;
    for (int i = startIndex; i < total; ++i) {
        const auto& entry = panel.moves[i];
        frame.put_text(formatElapsed(entry.elapsedMs), timeColX, y, 0.4, kMoveColor, 1);
        frame.put_text(entry.notation, moveColX, y, 0.4, kMoveColor, 1);
        y += rowHeight;
    }
}
}  // namespace

void OpenCvView::render(const std::vector<RenderPiece>& pieces, const BoardHighlight& highlight,
                         const Scoreboard& scoreboard) {
    Img frame = boardImg_.clone();

    drawPlayerPanel(frame, scoreboard.black, 0, sidePanelWidth_, height_);
    drawPlayerPanel(frame, scoreboard.white, sidePanelWidth_ + boardSize_, sidePanelWidth_, height_);

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
            drawRestRing(frame, mapper_, px, py, rp.cooldownMs, rp.cooldownTotalMs);
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
