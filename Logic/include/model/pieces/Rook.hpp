#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

class Rook : public Piece {
public:
    Rook(PlayerColor color, Position position);
};

}  // namespace kungfu
