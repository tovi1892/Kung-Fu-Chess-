#pragma once

#include "Piece.hpp"

namespace kungfu {

class Pawn : public Piece {
public:
    Pawn(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu
