#pragma once

#include <set>

#include "model/IBoard.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

// One stateless strategy class per piece type, computing legal destinations
// from Board + Piece data only. Enemy-occupied destinations may be legal
// (captures); these classes never mutate the board or the piece.
class IPieceRules {
public:
    virtual ~IPieceRules() = default;

    // Every square 'piece' could legally be requested toward right now, from
    // its own position/color/type and the board's current occupancy alone -
    // independent of anything happening elsewhere in real time.
    virtual std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const = 0;
};

// Horizontal/vertical sliding, any distance.
class RookRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

// Diagonal sliding, any distance.
class BishopRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

// Horizontal/vertical/diagonal sliding, any distance.
class QueenRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

// The 8 L-shaped jumps.
class KnightRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

// The 8 adjacent squares.
class KingRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

// One step forward (two from the start row), plus diagonal captures.
class PawnRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

// Returns the shared strategy instance for 'type' - the single factory every
// caller (RuleEngine) goes through instead of constructing rule objects directly.
const IPieceRules& pieceRulesFor(PieceType type);

}  // namespace kungfu
