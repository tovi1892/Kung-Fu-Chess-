#pragma once

#include "pieces/Piece.hpp"

namespace kungfu {

class Queen : public Piece {
public:
    Queen(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu
