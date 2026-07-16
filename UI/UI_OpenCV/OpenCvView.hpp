#pragma once

#include "IGameView.hpp"
#include "Img/img.hpp"
#include "AssetManager.hpp"
#include "Core/PieceAnimator.hpp"
#include "Core/CoordinateMapper.hpp"
#include <string>

namespace kungfu {

// The only IGameView implementation: an OpenCV window. See IGameView for
// what each overridden method does.
class OpenCvView : public IGameView {
public:
    OpenCvView(int width = 800, int height = 800);
    ~OpenCvView() override = default;

    void init() override;
    void render(const std::vector<kungfu::RenderPiece>& pieces, const BoardHighlight& highlight) override;
    bool isOpen() const override;
    void setInputHandler(IInputHandlerPtr handler) override;

private:
    static void onMouse(int event, int x, int y, int flags, void* userdata);
    // Renders the checkerboard + a-h/1-8 coordinate labels into boardImg_
    // once, from code - no background image file is involved.
    void drawBoard();

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
