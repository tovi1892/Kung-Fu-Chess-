#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

class Pawn : public Piece {
public:
    Pawn(PlayerColor color, Position position);

    bool isMovable() const override;
    bool isMoveValid(const Position& from, const Position& to) const override;
};

}  // namespace kungfu
