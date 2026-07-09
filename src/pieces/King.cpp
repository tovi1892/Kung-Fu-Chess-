#include "pieces/King.hpp"

#include <utility>

namespace kungfu {

King::King(PlayerColor color, Position position)
    : Piece(PieceType::King, color, std::move(position)) {}

bool King::isMovable() const {
    return true;
}

}  // namespace kungfu
