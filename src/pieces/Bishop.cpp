#include "pieces/Bishop.hpp"

#include <utility>

namespace kungfu {

Bishop::Bishop(PlayerColor color, Position position)
    : Piece(PieceType::Bishop, color, std::move(position)) {}

bool Bishop::isMovable() const {
    return true;
}

}  // namespace kungfu
