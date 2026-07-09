#include "game/UIInputAdapter.hpp"

#include <algorithm>
#include <cmath>

#include "game/Game.hpp"

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

    const int row = clampToBoard(y / 100, maxRows);
    const int col = clampToBoard(x / 100, maxCols);
    return Position(row, col);
}

UIInputAdapter::UIInputAdapter(Game& game) : game_(game) {}

void UIInputAdapter::setBoardMapper(BoardMapper mapper) {
    boardMapper_ = std::move(mapper);
}

void UIInputAdapter::handleClick(int x, int y) {
    if (!game_.isRunning()) {
        return;
    }

    const auto pos = boardMapper_.mapToBoard(x, y, 8, 8);
    if (!pos.has_value()) {
        return;
    }

    if (!game_.hasSelection()) {
        game_.selectPiece(*pos);
        return;
    }

    const auto selected = game_.selectedPosition();
    if (!selected.has_value()) {
        return;
    }

    if (*pos == *selected) {
        game_.requestJump(*pos);
        return;
    }

    if (game_.isFriendlyPieceAt(*pos)) {
        game_.selectPiece(*pos);
        return;
    }

    game_.requestMove(*selected, *pos);
}

std::optional<InputCommand> UIInputAdapter::nextCommand() {
    return std::nullopt;
}

}  // namespace kungfu
