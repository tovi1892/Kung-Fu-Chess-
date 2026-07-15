#include "model/pieces/Queen.hpp"

#include <cmath>
#include <utility>

namespace kungfu {

Queen::Queen(PlayerColor color, Position position)
    : Piece(PieceType::Queen, color, std::move(position)) {}

bool Queen::isMovable() const {
    return true;
}

bool Queen::isMoveValid(const Position& from, const Position& to) const {
    const int rowDelta = std::abs(to.row() - from.row());
    const int colDelta = std::abs(to.col() - from.col());
    return (rowDelta == 0 || colDelta == 0 || rowDelta == colDelta);
}

}  // namespace kungfu
