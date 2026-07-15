#pragma once

#include <set>

#include "model/IBoard.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

// Movement Rules layer (course spec section 7): one stateless strategy class
// per piece type, computing legal destinations from Board + Piece data only.
// Enemy-occupied destinations may be legal (captures); these classes never
// mutate the board or the piece.
class IPieceRules {
public:
    virtual ~IPieceRules() = default;
    virtual std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const = 0;
};

class RookRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

class BishopRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

class QueenRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

class KnightRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

class KingRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

class PawnRules : public IPieceRules {
public:
    std::set<Position> legalDestinations(const IBoard& board, const Piece& piece) const override;
};

// Strategy-per-piece-type factory (Pattern vocabulary table, section 5).
const IPieceRules& pieceRulesFor(PieceType type);

}  // namespace kungfu
