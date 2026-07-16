#pragma once

#include <optional>
#include "model/Position.hpp"

namespace kungfu {

// Small, stateless bounds-check helpers shared by a couple of call sites -
// not a per-piece rules layer (see rules/PieceRules for that).
class MovementSystem {
public:
    // True when 'position' is within the fixed GameConfig::kBoardSize board.
    bool isInBounds(const Position& position) const;

    // True when 'position' is within a board of the given size (for boards
    // that aren't the fixed 8x8, e.g. one built by BoardParser).
    bool isInBounds(const Position& position, int rows, int cols) const;

    bool isSamePosition(const Position& from, const Position& to) const;

    // True when both squares are in bounds and 'from' != 'to'.
    bool canMoveTo(const Position& from, const Position& to) const;

    // The square skipped over by a straight 2-row pawn move (for
    // en-passant-style logic), or nullopt if 'from'->'to' isn't one.
    std::optional<Position> pawnDoubleStepMiddle(const Position& from, const Position& to) const;
};

}  // namespace kungfu