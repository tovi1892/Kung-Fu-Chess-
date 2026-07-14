#pragma once

#include "pieces/Piece.hpp"

namespace kungfu {

class Bishop : public Piece {
public:
    Bishop(PlayerColor color, Position position);

    bool isMovable() const override;
    bool isMoveValid(const Position& from, const Position& to) const override;
};

}  // namespace kungfu
