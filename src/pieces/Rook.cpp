#include "pieces/Rook.hpp"

#include <utility>

namespace kungfu {

Rook::Rook(PlayerColor color, Position position)
    : Piece(PieceType::Rook, color, std::move(position)) {}

bool Rook::isMovable() const {
    return true;
}

}  // namespace kungfu
