#pragma once

#include "Piece.hpp"

namespace kungfu {

class Bishop : public Piece {
public:
    Bishop(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu
