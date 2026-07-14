#pragma once

#include "IGameView.hpp"
#include "Img/img.hpp"
#include "AssetManager.hpp"
#include "CoordinateMapper.hpp"
#include <string>

namespace kungfu {

class OpenCvView : public IGameView {
public:
    OpenCvView(int width = 800, int height = 800);
    ~OpenCvView() override = default;

    void init() override;
    void render(const std::vector<kungfu::RenderPiece>& pieces) override;
    bool isOpen() const override;
    void setInputHandler(IInputHandlerPtr handler) override;

private:
    static void onMouse(int event, int x, int y, int flags, void* userdata);

    int width_;
    int height_;
    std::string windowName_;
    Img boardImg_;
    AssetManager assets_;
    CoordinateMapper mapper_;
    IInputHandlerPtr inputHandler_;
    bool closed_ = false;
};

} // namespace kungfu
