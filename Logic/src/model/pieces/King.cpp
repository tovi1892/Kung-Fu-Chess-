#include "model/pieces/King.hpp"

#include <cmath>
#include <utility>

namespace kungfu {

King::King(PlayerColor color, Position position)
    : Piece(PieceType::King, color, std::move(position)) {}

bool King::isMovable() const {
    return true;
}

bool King::isMoveValid(const Position& from, const Position& to) const {
    const int rowDelta = std::abs(to.row() - from.row());
    const int colDelta = std::abs(to.col() - from.col());
    return (rowDelta <= 1 && colDelta <= 1);
}

}  // namespace kungfu
