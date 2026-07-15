#include "model/pieces/Bishop.hpp"

#include <utility>

namespace kungfu {

Bishop::Bishop(PlayerColor color, Position position)
    : Piece(PieceType::Bishop, color, std::move(position)) {}

}  // namespace kungfu
