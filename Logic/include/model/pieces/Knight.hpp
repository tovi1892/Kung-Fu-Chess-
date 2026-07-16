#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

// A thin identity subclass - see King.hpp for why this exists. Carries no
// movement rules or other behavior of its own.
class Knight : public Piece {
public:
    Knight(PlayerColor color, Position position);
};

}  // namespace kungfu
