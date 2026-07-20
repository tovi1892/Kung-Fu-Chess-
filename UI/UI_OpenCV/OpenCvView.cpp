#include "OpenCvView.hpp"

#include "BoardRenderer.hpp"
#include "ScoreboardRenderer.hpp"
#include "model/GameConfig.hpp"

namespace kungfu {

namespace {
// OpenCV doesn't provide a named constant for this - 27 is Escape's ASCII code.
constexpr int kEscapeKeyCode = 27;
}  // namespace

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
    static const cv::Scalar kPanelBg(20, 20, 20);
    boardImg_.create(width_, height_, kPanelBg);

    drawCheckerboardAndLabels(boardImg_, mapper_);

    static const cv::Scalar kDividerColor(75, 75, 75);
    boardImg_.draw_rect(sidePanelWidth_ - RenderConfig::kDividerThicknessPx, 0, RenderConfig::kDividerThicknessPx,
                         height_, kDividerColor);
    boardImg_.draw_rect(sidePanelWidth_ + boardSize_, 0, RenderConfig::kDividerThicknessPx, height_, kDividerColor);
}

void OpenCvView::render(const std::vector<RenderPiece>& pieces, const BoardHighlight& highlight,
                         const Scoreboard& scoreboard, const Banner& banner) {
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
        drawPiece(frame, rp, assets_, animator_, mapper_);
    }

    if (banner.visible) {
        drawBanner(frame, mapper_, banner.text);
    }

    cv::imshow(windowName_, frame.get_mat());

    const int key = cv::waitKey(RenderConfig::kFrameWaitMs);
    if (key == kEscapeKeyCode || cv::getWindowProperty(windowName_, cv::WND_PROP_VISIBLE) < 1) {
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
