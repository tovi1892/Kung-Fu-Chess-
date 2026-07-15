#include "model/pieces/Pawn.hpp"

#include <utility>

namespace kungfu {

Pawn::Pawn(PlayerColor color, Position position)
    : Piece(PieceType::Pawn, color, std::move(position)) {}

}  // namespace kungfu
