#include "game/UIInputAdapter.hpp"

#include <algorithm>
#include <cmath>

#include "model/GameConfig.hpp"
#include "game/GameController.hpp"

namespace kungfu {

namespace {

int clampToBoard(int value, int maxBound) {
    if (value < 0) {
        return 0;
    }
    if (value >= maxBound) {
        return maxBound - 1;
    }
    return value;
}

}  // namespace

std::optional<Position> BoardMapper::mapToBoard(int x, int y, int maxRows, int maxCols) const {
    if (maxRows <= 0 || maxCols <= 0) {
        return std::nullopt;
    }

    const int row = clampToBoard(y / GameConfig::kCellSizePx, maxRows);
    const int col = clampToBoard(x / GameConfig::kCellSizePx, maxCols);
    return Position(row, col);
}

UIInputAdapter::UIInputAdapter(IGameInputTarget& game)
    : targetProvider_([&game]() -> IGameInputTarget& { return game; }) {}

UIInputAdapter::UIInputAdapter(std::function<IGameInputTarget&()> targetProvider)
    : targetProvider_(std::move(targetProvider)) {}

void UIInputAdapter::setBoardMapper(BoardMapper mapper) {
    boardMapper_ = std::move(mapper);
}

void UIInputAdapter::handleClick(int x, int y) {
    auto& game = targetProvider_();
    if (!game.isRunning()) {
        return;
    }

    const auto pos = boardMapper_.mapToBoard(x, y, game.boardRows(), game.boardCols());
    if (!pos.has_value()) {
        return;
    }

    if (auto* controller = dynamic_cast<GameController*>(&game)) {
        controller->handleCellClick(pos->row(), pos->col());
        return;
    }

    if (!game.hasSelection()) {
        game.selectPiece(*pos);
        return;
    }

    const auto selected = game.selectedPosition();
    if (!selected.has_value()) {
        return;
    }

    if (*pos == *selected) {
        game.requestJump(*pos);
        return;
    }

    if (game.isFriendlyPieceAt(*pos)) {
        game.selectPiece(*pos);
        return;
    }

    game.requestMove(*selected, *pos);
}

std::optional<InputCommand> UIInputAdapter::nextCommand() {
    return std::nullopt;
}

}  // namespace kungfu
