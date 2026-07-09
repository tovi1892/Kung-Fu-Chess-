#pragma once

#include "pieces/Piece.hpp"

namespace kungfu {

class King : public Piece {
public:
    King(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu
