#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

class Queen : public Piece {
public:
    Queen(PlayerColor color, Position position);
};

}  // namespace kungfu
