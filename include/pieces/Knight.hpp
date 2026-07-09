#pragma once

#include "pieces/Piece.hpp"

namespace kungfu {

class Knight : public Piece {
public:
    Knight(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu
