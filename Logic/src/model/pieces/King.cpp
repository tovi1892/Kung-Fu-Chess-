#include "model/pieces/King.hpp"

#include <utility>

namespace kungfu {

King::King(PlayerColor color, Position position)
    : Piece(PieceType::King, color, std::move(position)) {}

}  // namespace kungfu
