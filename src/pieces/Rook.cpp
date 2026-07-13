#include "pieces/Rook.hpp"

#include <cmath>
#include <utility>

namespace kungfu {

Rook::Rook(PlayerColor color, Position position)
    : Piece(PieceType::Rook, color, std::move(position)) {}

bool Rook::isMovable() const {
    return true;
}

bool Rook::isMoveValid(const Position& from, const Position& to) const {
    const int rowDelta = std::abs(to.row() - from.row());
    const int colDelta = std::abs(to.col() - from.col());
    return (rowDelta == 0 || colDelta == 0);
}

}  // namespace kungfu
