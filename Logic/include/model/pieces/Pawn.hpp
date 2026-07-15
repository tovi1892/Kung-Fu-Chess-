#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

class Pawn : public Piece {
public:
    Pawn(PlayerColor color, Position position);
};

}  // namespace kungfu
