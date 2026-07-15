#include "OpenCvView.hpp"

#include <sstream>

#include "model/GameConfig.hpp"

namespace kungfu {

OpenCvView::OpenCvView(int width, int height)
    : width_(width), height_(height), windowName_("KungFuChess"),
      mapper_(width, height, GameConfig::kBoardSize, GameConfig::kBoardSize) {}

void OpenCvView::init() {
    boardImg_.read("UI/assets/board.png", {width_, height_}, true, cv::INTER_LINEAR);

    cv::namedWindow(windowName_, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(windowName_, &OpenCvView::onMouse, this);
}

void OpenCvView::render(const std::vector<RenderPiece>& pieces) {
    Img frame = boardImg_.clone();

    for (const auto& rp : pieces) {
        const int row = static_cast<int>(rp.x + 0.5);
        const int col = static_cast<int>(rp.y + 0.5);
        const int px = mapper_.cellTopLeftX(col);
        const int py = mapper_.cellTopLeftY(row);

        auto& sprite = assets_.getPieceSprite(static_cast<PieceType>(rp.type),
                                               static_cast<PlayerColor>(rp.color),
                                               mapper_.cellWidth(), mapper_.cellHeight());
        sprite.draw_on(frame, px, py);

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
