#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

class King : public Piece {
public:
    King(PlayerColor color, Position position);
};

}  // namespace kungfu
