#include <iostream>
#include <memory>
#include <sstream>

#include "BoardParser.hpp"
#include "model/Board.hpp"
#include "model/GameConfig.hpp"
#include "game/Game.hpp"
#include "game/GameController.hpp"
#include "rules/RuleEngine.hpp"

#include "CoordinateMapper.hpp"
#include "UI_OpenCV/OpenCvView.hpp"
#include "IInputHandler.hpp"

using namespace kungfu;

namespace {

// Row order follows GameConfig: White starts at row 1 and advances toward row 7
// (kWhitePawnStartRow/kWhitePawnPromotionRow), Black starts at row 6 and advances toward row 0.
const char* kInitialBoard = R"(Board:
wR wN wB wQ wK wB wN wR
wP wP wP wP wP wP wP wP
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
bP bP bP bP bP bP bP bP
bR bN bB bQ bK bB bN bR
)";

// Translates raw pixel clicks from the view into board-cell clicks on the controller.
class BoardClickHandler : public IInputHandler {
public:
    BoardClickHandler(GameController& controller, CoordinateMapper mapper)
        : controller_(controller), mapper_(mapper) {}

    void handleClick(int x, int y) override {
        const Position cell = mapper_.pixelToCell(x, y);
        controller_.handleCellClick(cell.row(), cell.col());
    }

    std::optional<int> pollEvent() override { return std::nullopt; }

private:
    GameController& controller_;
    CoordinateMapper mapper_;
};

}  // namespace

int main() {
    std::shared_ptr<Board> board;
    std::istringstream boardStream(kInitialBoard);
    if (!BoardParser().parseInput(boardStream, board)) {
        std::cerr << "Failed to parse the initial board layout" << std::endl;
        return 1;
    }

    auto ruleEngine = std::make_shared<RuleEngine>(board);
    auto game = std::make_shared<Game>(board, ruleEngine);
    game->start();

    auto controller = std::make_shared<GameController>(game);

    const int windowSize = 800;
    CoordinateMapper mapper(windowSize, windowSize, GameConfig::kBoardSize, GameConfig::kBoardSize);
    auto clickHandler = std::make_shared<BoardClickHandler>(*controller, mapper);

    OpenCvView view(windowSize, windowSize);
    view.setInputHandler(clickHandler);
    view.init();

    while (view.isOpen()) {
        controller->handleTimePassed(16);
        view.render(game->getRenderState());
    }

    return 0;
}
