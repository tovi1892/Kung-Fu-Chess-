#pragma once

#include <string>
#include "model/Enums.hpp"
#include "model/Position.hpp"

namespace kungfu {

// is_valid is always paired with a stable, machine-readable reason: "ok" on
// success, or one of "outside_board" / "empty_source" / "friendly_destination"
// / "illegal_piece_move" on failure.
struct MoveValidation {
    bool is_valid;
    std::string reason;
};

// Read-only move legality service, built on top of PieceRules. Never mutates
// the board.
class IRuleEngine {
public:
    virtual ~IRuleEngine() = default;

    // Checks whether moving whatever is at 'from' to 'to' is legal right
    // now, and why not if it isn't. See MoveValidation above.
    virtual MoveValidation validateMove(const Position& from, const Position& to) const = 0;

    // Shorthand for validateMove(from, to).is_valid.
    virtual bool isValidMove(const Position& from, const Position& to) const = 0;

    // Returns true if a pawn at 'to' with the given color should be promoted.
    virtual bool isPawnPromotion(const Position& to, PlayerColor color) const = 0;
};

}  // namespace kungfu
