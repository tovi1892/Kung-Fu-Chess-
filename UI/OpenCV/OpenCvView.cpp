#include "OpenCvView.hpp"

#include <algorithm>

#include "BoardRenderer.hpp"
#include "Img/Color.hpp"
#include "ScoreboardRenderer.hpp"
#include "model/GameConfig.hpp"

namespace kungfu {

namespace {
// OpenCV doesn't provide a named constant for this - 27 is Escape's ASCII code.
constexpr int kEscapeKeyCode = 27;
// Shared by drawStaticBackground() (the checkered board's panel backdrop) and render()'s
// letterbox composite (see the class comment on realWidth_/realHeight_) - one color, one
// definition, rather than duplicating the literal in both places.
constexpr Color kPanelBg{20, 20, 20};
}  // namespace

OpenCvView::OpenCvView(int boardSize, int sidePanelWidth)
    : referenceBoardSize_(boardSize),
      referenceSidePanelWidth_(sidePanelWidth),
      referenceWidth_(sidePanelWidth * 2 + boardSize),
      referenceHeight_(boardSize + RenderConfig::kTopStripHeightPx),
      scale_(referenceWidth_, referenceHeight_, referenceWidth_, referenceHeight_),  // 1.0 factor at construction
      boardSize_(referenceBoardSize_),
      sidePanelWidth_(referenceSidePanelWidth_),
      width_(referenceWidth_),
      height_(referenceHeight_),
      realWidth_(referenceWidth_),
      realHeight_(referenceHeight_),
      windowName_("KungFuChess"),
      mapper_(boardSize_, boardSize_, GameConfig::kBoardSize, GameConfig::kBoardSize,
              CoordinateMapper::kDefaultMargin, sidePanelWidth_, RenderConfig::kTopStripHeightPx) {}

void OpenCvView::init() {
    drawStaticBackground();

    // WINDOW_NORMAL (not WINDOW_AUTOSIZE) is what makes the window user-resizable at all;
    // explicitly resizing to the reference size right away avoids depending on whatever
    // default size the OS/HighGUI backend would otherwise have picked.
    cv::namedWindow(windowName_, cv::WINDOW_NORMAL);
    cv::resizeWindow(windowName_, width_, height_);
    cv::setMouseCallback(windowName_, &OpenCvView::onMouse, this);
}

void OpenCvView::drawStaticBackground() {
    boardImg_.create(width_, height_, kPanelBg);

    drawCheckerboardAndLabels(boardImg_, mapper_, scale_);

    static constexpr Color kDividerColor{75, 75, 75};
    const int dividerThickness = scale_.px(RenderConfig::kDividerThicknessPx);
    boardImg_.draw_rect(sidePanelWidth_ - dividerThickness, 0, dividerThickness, height_, kDividerColor);
    boardImg_.draw_rect(sidePanelWidth_ + boardSize_, 0, dividerThickness, height_, kDividerColor);
}

void OpenCvView::handlePossibleResize() {
    const cv::Rect windowRect = cv::getWindowImageRect(windowName_);
    if (windowRect.width <= 0 || windowRect.height <= 0) {
        return;  // window not fully created/visible yet - nothing to do
    }
    if (windowRect.width == realWidth_ && windowRect.height == realHeight_) {
        return;  // no change since the last check - true on almost every frame
    }

    realWidth_ = windowRect.width;
    realHeight_ = windowRect.height;
    scale_ = LayoutScale(realWidth_, realHeight_, referenceWidth_, referenceHeight_);
    boardSize_ = scale_.px(referenceBoardSize_);
    sidePanelWidth_ = scale_.px(referenceSidePanelWidth_);
    width_ = sidePanelWidth_ * 2 + boardSize_;
    height_ = boardSize_ + scale_.px(RenderConfig::kTopStripHeightPx);
    // width_/height_ are each the SUM of independently-rounded sub-components (two side
    // panels + the board; the board + the top strip) - each term rounds to its own nearest
    // pixel, so the sum can drift a pixel or two ABOVE what the scale factor alone would
    // give, even though it's derived from realWidth_/realHeight_. Clamping guarantees the
    // content frame never exceeds the real window, which render()'s letterbox composite
    // depends on: a negative offset would otherwise crash Img::draw_on (its ROI extraction
    // requires the rect fully within bounds, unlike draw_rect/put_text, which clip safely).
    width_ = std::min(width_, realWidth_);
    height_ = std::min(height_, realHeight_);
    contentOffsetX_ = (realWidth_ - width_) / 2;
    contentOffsetY_ = (realHeight_ - height_) / 2;

    // mapper_ stays content-frame-relative (matching boardImg_/frame, which are built at
    // the smaller content size, not the real window size) - see mapper()'s own doc comment
    // for why callers translating a raw mouse coordinate need contentOffsetX()/Y() too.
    mapper_ = CoordinateMapper(boardSize_, boardSize_, GameConfig::kBoardSize, GameConfig::kBoardSize,
                               scale_.px(CoordinateMapper::kDefaultMargin), sidePanelWidth_,
                               scale_.px(RenderConfig::kTopStripHeightPx));
    drawStaticBackground();
}

void OpenCvView::render(const std::vector<RenderPiece>& pieces, const BoardHighlight& highlight,
                         const Scoreboard& scoreboard, const Banner& banner) {
    handlePossibleResize();

    Img frame = boardImg_.clone();

    drawRoomLabel(frame, scoreboard.roomLabel, scale_, width_);
    drawPlayerPanel(frame, scoreboard.black, scale_, 0, sidePanelWidth_, height_);
    drawPlayerPanel(frame, scoreboard.white, scale_, sidePanelWidth_ + boardSize_, sidePanelWidth_, height_);

    static constexpr Color kSelectionColor{255, 255, 255};   // white - the square currently selected
    static constexpr Color kLastMoveColor{80, 220, 80};      // green - the last move's from/to squares

    if (highlight.hasSelection) {
        drawCellOutline(frame, mapper_, scale_, highlight.selectedRow, highlight.selectedCol, kSelectionColor);
    }
    if (highlight.hasLastMove) {
        drawCellOutline(frame, mapper_, scale_, highlight.lastMoveFromRow, highlight.lastMoveFromCol, kLastMoveColor);
        drawCellOutline(frame, mapper_, scale_, highlight.lastMoveToRow, highlight.lastMoveToCol, kLastMoveColor);
    }

    for (const auto& rp : pieces) {
        drawPiece(frame, rp, assets_, animator_, mapper_, scale_);
    }

    if (banner.visible) {
        drawBanner(frame, mapper_, scale_, banner.text);
    }

    // frame is drawn at the *content* size (width_/height_), which can be smaller than the
    // real OS window (realWidth_/realHeight_) if the window's live aspect ratio doesn't
    // match the reference one - WINDOW_NORMAL's imshow stretches whatever it's given to
    // fill the window, so handing it a smaller image directly would silently re-introduce
    // the exact distortion LayoutScale's uniform factor was meant to avoid. Compositing
    // onto a real-window-sized, background-colored canvas first (only when the two sizes
    // actually differ) turns that slack into genuine letterbox bars instead.
    if (realWidth_ != width_ || realHeight_ != height_) {
        Img canvas;
        canvas.create(realWidth_, realHeight_, kPanelBg);
        frame.draw_on(canvas, contentOffsetX_, contentOffsetY_);
        cv::imshow(windowName_, canvas.get_mat());
    } else {
        cv::imshow(windowName_, frame.get_mat());
    }

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
