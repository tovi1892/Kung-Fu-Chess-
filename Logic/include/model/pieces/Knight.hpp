#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

class Knight : public Piece {
public:
    Knight(PlayerColor color, Position position);
};

}  // namespace kungfu
