#pragma once

#include "model/pieces/Piece.hpp"

namespace kungfu {

// A thin identity subclass: exists only so a king can be constructed and
// referred to as a King rather than a plain Piece(PieceType::King, ...).
// Carries no movement rules or other behavior of its own.
class King : public Piece {
public:
    King(PlayerColor color, Position position);
};

}  // namespace kungfu
