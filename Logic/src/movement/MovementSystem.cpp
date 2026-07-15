#include "movement/MovementSystem.hpp"
#include <cmath>
#include "model/GameConfig.hpp"

namespace kungfu {

bool MovementSystem::isInBounds(const Position& position) const {
    return position.row() >= 0 && position.row() < GameConfig::kBoardSize &&
           position.col() >= 0 && position.col() < GameConfig::kBoardSize;
}

bool MovementSystem::isInBounds(const Position& position, int rows, int cols) const {
    return position.row() >= 0 && position.row() < rows &&
           position.col() >= 0 && position.col() < cols;
}

bool MovementSystem::isSamePosition(const Position& from, const Position& to) const {
    return from == to;
}

bool MovementSystem::canMoveTo(const Position& from, const Position& to) const {
    return isInBounds(from) && isInBounds(to) && !isSamePosition(from, to);
}

std::optional<Position> MovementSystem::pawnDoubleStepMiddle(const Position& from, const Position& to) const {
    const int rowDelta = to.row() - from.row();
    if (std::abs(rowDelta) == 2 && from.col() == to.col()) {
        return Position(from.row() + rowDelta / 2, from.col());
    }
    return std::nullopt;
}

}  // namespace kungfu