#pragma once

#include "IGameView.hpp"
#include "Img/img.hpp"
#include "AssetManager.hpp"
#include "PieceAnimator.hpp"
#include "Geometry/CoordinateMapper.hpp"
#include "Geometry/LayoutScale.hpp"
#include "RenderConfig.hpp"
#include <string>

namespace kungfu {

// The only IGameView implementation: an OpenCV window. See IGameView for
// what each overridden method does. The window is laid out as [left side
// panel][board][right side panel]; boardSize is the board's own square
// extent, sidePanelWidth each panel's width - both taken as the *reference*
// (100%-scale) sizes; the window is user-resizable (WINDOW_NORMAL, not
// WINDOW_AUTOSIZE) and everything scales from a LayoutScale computed against
// these reference values as the real OS window size changes.
class OpenCvView : public IGameView {
public:
    OpenCvView(int boardSize = RenderConfig::kDefaultWindowBoardSizePx,
               int sidePanelWidth = RenderConfig::kDefaultSidePanelWidthPx);
    ~OpenCvView() override = default;

    void init() override;
    void render(const std::vector<kungfu::RenderPiece>& pieces, const BoardHighlight& highlight,
                const Scoreboard& scoreboard, const Banner& banner) override;
    bool isOpen() const override;
    void setInputHandler(IInputHandlerPtr handler) override;

    // Read-only access to the pixel<->cell mapper this view uses internally,
    // so the composition root (main.cpp) can translate raw mouse clicks into
    // board cells without constructing (and having to keep in sync) a
    // second, separate CoordinateMapper. Note this can change (a new
    // CoordinateMapper value) across a resize - callers that hold onto a copy across
    // frames rather than re-reading this accessor each time will go stale.
    //
    // mapper() is *content-frame*-relative (0,0 at the top-left of boardImg_/frame, the
    // possibly-letterboxed, possibly-smaller-than-the-real-window canvas everything is
    // actually drawn onto - see width_/height_ vs realWidth_/realHeight_ below) - it is
    // NOT real-OS-window-relative. Raw mouse coordinates from cv::setMouseCallback *are*
    // real-window-relative, so callers must subtract contentOffsetX()/contentOffsetY()
    // before calling mapper().pixelToCell() on them, or clicks will be off by exactly the
    // letterbox margin whenever one is showing.
    const CoordinateMapper& mapper() const { return mapper_; }

    // The letterbox centering offset last applied when compositing the content frame onto
    // the real window (0,0 when the window's live aspect ratio matches the reference one,
    // i.e. no letterboxing needed) - see mapper()'s own comment for why callers translating
    // a raw mouse coordinate need this.
    int contentOffsetX() const { return contentOffsetX_; }
    int contentOffsetY() const { return contentOffsetY_; }

private:
    static void onMouse(int event, int x, int y, int flags, void* userdata);
    // Builds boardImg_ once, from code (no background image file is
    // involved): the panel background/dividers directly, delegating the
    // checkerboard + a-h/1-8 labels to BoardRenderer. Player name/score/
    // move-list content is dynamic and drawn fresh every render() call
    // instead (see ScoreboardRenderer).
    void drawStaticBackground();
    // Checks the real OS window size (cv::getWindowImageRect) against what boardImg_ was
    // last built for; if it changed (a user drag-resize), recomputes scale_ and every
    // scaled dimension from it, rebuilds mapper_, and redraws boardImg_ at the new content
    // size - all before this frame's dynamic content gets drawn on top. A no-op most
    // frames (nothing to do until the OS actually reports a new size).
    void handlePossibleResize();

    // Reference (100%-scale) sizes, fixed for the object's lifetime - LayoutScale computes
    // every resize's actual scale factor against these, never against the previous size.
    const int referenceBoardSize_;
    const int referenceSidePanelWidth_;
    const int referenceWidth_;
    const int referenceHeight_;

    LayoutScale scale_;
    // Current *content* size - i.e. what boardImg_ is actually drawn at. May be smaller
    // than the real OS window (realWidth_/realHeight_ below) if the window's live aspect
    // ratio doesn't match the reference one - see render()'s letterboxing composite.
    int boardSize_;
    int sidePanelWidth_;
    int width_;
    int height_;
    // The real OS window's size as of the last handlePossibleResize() check - compared
    // against on every frame to detect a drag-resize, and against width_/height_ each frame
    // to decide whether render() needs to letterbox-composite before imshow.
    int realWidth_;
    int realHeight_;
    // (realWidth_ - width_) / 2 and (realHeight_ - height_) / 2 respectively - recomputed
    // only in handlePossibleResize(), reused as-is by both render()'s compositing and
    // contentOffsetX()/contentOffsetY()'s callers, so the two are never at risk of drifting
    // apart via separately-recomputed arithmetic.
    int contentOffsetX_ = 0;
    int contentOffsetY_ = 0;

    std::string windowName_;
    Img boardImg_;
    AssetManager assets_;
    PieceAnimator animator_;
    CoordinateMapper mapper_;
    IInputHandlerPtr inputHandler_;
    bool closed_ = false;
};

} // namespace kungfu
