#include "model/pieces/Bishop.hpp"

#include <cmath>
#include <utility>

namespace kungfu {

Bishop::Bishop(PlayerColor color, Position position)
    : Piece(PieceType::Bishop, color, std::move(position)) {}

bool Bishop::isMovable() const {
    return true;
}

bool Bishop::isMoveValid(const Position& from, const Position& to) const {
    const int rowDelta = std::abs(to.row() - from.row());
    const int colDelta = std::abs(to.col() - from.col());
    return (rowDelta == colDelta);
}

}  // namespace kungfu
