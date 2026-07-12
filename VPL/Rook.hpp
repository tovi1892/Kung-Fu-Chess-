#pragma once

#include "Piece.hpp"

namespace kungfu {

class Rook : public Piece {
public:
    Rook(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu
