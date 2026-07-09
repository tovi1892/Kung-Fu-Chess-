#include "pieces/Pawn.hpp"

#include <utility>

namespace kungfu {

Pawn::Pawn(PlayerColor color, Position position)
    : Piece(PieceType::Pawn, color, std::move(position)) {}

bool Pawn::isMovable() const {
    return true;
}

}  // namespace kungfu
