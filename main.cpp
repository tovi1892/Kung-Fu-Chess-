#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>

#include "io/BoardParser.hpp"
#include "model/Board.hpp"
#include "model/GameConfig.hpp"
#include "engine/GameEngine.hpp"
#include "input/Controller.hpp"
#include "rules/RuleEngine.hpp"

#include "Core/CoordinateMapper.hpp"
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

// How long the last-move highlight stays visible after a move/jump fires.
// Purely cosmetic, so it lives here rather than in Controller (which only
// owns selection state) or GameEngine (which has no concept of "recent").
constexpr std::chrono::milliseconds kLastMoveHighlightDuration{1000};

// Translates raw pixel clicks from the view into board-cell clicks on the
// controller, and separately remembers the most recent accepted move/jump
// purely so main()'s render loop can show a brief "these are the squares
// that just moved" highlight - a presentation concern with no effect on
// gameplay, so it's tracked here instead of inside Controller or GameEngine.
class BoardClickHandler : public IInputHandler {
public:
    BoardClickHandler(Controller& controller, CoordinateMapper mapper)
        : controller_(controller), mapper_(mapper) {}

    void handleClick(int x, int y) override {
        const Position cell = mapper_.pixelToCell(x, y);
        const auto from = controller_.selectedPosition();

        const bool accepted = controller_.handleCellClick(cell.row(), cell.col());

        if (accepted && from.has_value()) {
            lastMoveFrom_ = *from;
            lastMoveTo_ = cell;
            lastMoveAt_ = std::chrono::steady_clock::now();
        }
    }

    std::optional<int> pollEvent() override { return std::nullopt; }

    // Only returns a value while the most recent move/jump is still fresh
    // (see kLastMoveHighlightDuration) - main() treats "no value" as
    // "nothing to highlight".
    std::optional<std::pair<Position, Position>> recentMove() const {
        if (!lastMoveFrom_.has_value()) {
            return std::nullopt;
        }
        const auto elapsed = std::chrono::steady_clock::now() - lastMoveAt_;
        if (elapsed > kLastMoveHighlightDuration) {
            return std::nullopt;
        }
        return std::make_pair(*lastMoveFrom_, *lastMoveTo_);
    }

private:
    Controller& controller_;
    CoordinateMapper mapper_;
    std::optional<Position> lastMoveFrom_;
    std::optional<Position> lastMoveTo_;
    std::chrono::steady_clock::time_point lastMoveAt_;
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
    auto game = std::make_shared<GameEngine>(board, ruleEngine);
    game->start();

    auto controller = std::make_shared<Controller>(game);

    const int windowSize = 800;
    CoordinateMapper mapper(windowSize, windowSize, GameConfig::kBoardSize, GameConfig::kBoardSize);
    auto clickHandler = std::make_shared<BoardClickHandler>(*controller, mapper);

    OpenCvView view(windowSize, windowSize);
    view.setInputHandler(clickHandler);
    view.init();

    while (view.isOpen()) {
        controller->handleTimePassed(16);

        BoardHighlight highlight;
        if (const auto selected = controller->selectedPosition(); selected.has_value()) {
            highlight.hasSelection = true;
            highlight.selectedRow = selected->row();
            highlight.selectedCol = selected->col();
        }
        if (const auto recent = clickHandler->recentMove(); recent.has_value()) {
            highlight.hasLastMove = true;
            highlight.lastMoveFromRow = recent->first.row();
            highlight.lastMoveFromCol = recent->first.col();
            highlight.lastMoveToRow = recent->second.row();
            highlight.lastMoveToCol = recent->second.col();
        }

        view.render(game->getRenderState(), highlight);
    }

    return 0;
}
