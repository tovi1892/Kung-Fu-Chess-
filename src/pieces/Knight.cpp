#include "pieces/Knight.hpp"

#include <utility>

namespace kungfu {

Knight::Knight(PlayerColor color, Position position)
    : Piece(PieceType::Knight, color, std::move(position)) {}

bool Knight::isMovable() const {
    return true;
}

}  // namespace kungfu
