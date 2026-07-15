#include "model/pieces/Queen.hpp"

#include <utility>

namespace kungfu {

Queen::Queen(PlayerColor color, Position position)
    : Piece(PieceType::Queen, color, std::move(position)) {}

}  // namespace kungfu
