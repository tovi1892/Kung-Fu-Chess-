#include "pieces/Knight.hpp"

#include <cmath>
#include <utility>

namespace kungfu {

Knight::Knight(PlayerColor color, Position position)
    : Piece(PieceType::Knight, color, std::move(position)) {}

bool Knight::isMovable() const {
    return true;
}

bool Knight::isMoveValid(const Position& from, const Position& to) const {
    const int rowDelta = std::abs(to.row() - from.row());
    const int colDelta = std::abs(to.col() - from.col());
    return (rowDelta == 2 && colDelta == 1) || (rowDelta == 1 && colDelta == 2);
}

}  // namespace kungfu
