#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

class Bishop : public Piece {
public:
    Bishop(PlayerColor color, Position position);
};

}  // namespace kungfu
