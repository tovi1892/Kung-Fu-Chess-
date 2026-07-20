#pragma once

#include "IGameView.hpp"
#include "Img/img.hpp"
#include "AssetManager.hpp"
#include "PieceAnimator.hpp"
#include "Geometry/CoordinateMapper.hpp"
#include "RenderConfig.hpp"
#include <string>

namespace kungfu {

// The only IGameView implementation: an OpenCV window. See IGameView for
// what each overridden method does. The window is laid out as [left side
// panel][board][right side panel]; boardSize is the board's own square
// extent, sidePanelWidth each panel's width.
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
    // second, separate CoordinateMapper.
    const CoordinateMapper& mapper() const { return mapper_; }

private:
    static void onMouse(int event, int x, int y, int flags, void* userdata);
    // Builds boardImg_ once, from code (no background image file is
    // involved): the panel background/dividers directly, delegating the
    // checkerboard + a-h/1-8 labels to BoardRenderer. Player name/score/
    // move-list content is dynamic and drawn fresh every render() call
    // instead (see ScoreboardRenderer).
    void drawStaticBackground();

    int boardSize_;
    int sidePanelWidth_;
    int width_;
    int height_;
    std::string windowName_;
    Img boardImg_;
    AssetManager assets_;
    PieceAnimator animator_;
    CoordinateMapper mapper_;
    IInputHandlerPtr inputHandler_;
    bool closed_ = false;
};

} // namespace kungfu
