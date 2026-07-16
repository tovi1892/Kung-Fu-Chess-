#pragma once

#include <string>
#include <vector>

#include "model/Enums.hpp"
#include "model/Position.hpp"

namespace kungfu {

// Point value of a captured piece, for scoring: knight/bishop = 3, rook = 5,
// queen = 9, pawn = 1, king = 0 (capturing a king ends the game before a
// score would matter, but it's still a real PieceType so this stays total).
int pieceValue(PieceType type);

// Builds a short algebraic-style notation string for a move: just the
// destination square for a pawn ("e5"), or a piece letter + destination for
// anything else ("Ke7"), with an "x" inserted before the destination (and,
// for a pawn, its origin file prepended) when the move is a capture.
// Notation is built from the requested move, not wherever real-time
// resolution actually ends it - a simplification, not a full PGN engine.
std::string moveNotation(PieceType type, const Position& from, const Position& to, bool isCapture);

// One recorded move: when it happened (elapsed game time, ms) and its notation.
struct MoveRecord {
    int elapsedMs;
    std::string notation;
};

// Per-color move history and score. Purely a ledger - accumulates whatever
// GameEngine reports (accepted moves, resolved captures) without knowing
// anything about board state or real-time timing itself.
class GameRecord {
public:
    void recordMove(PlayerColor color, int elapsedMs, const std::string& notation);
    void recordCapture(PlayerColor capturingColor, PieceType capturedType);

    const std::vector<MoveRecord>& movesFor(PlayerColor color) const;
    int scoreFor(PlayerColor color) const;

private:
    std::vector<MoveRecord> whiteMoves_;
    std::vector<MoveRecord> blackMoves_;
    int whiteScore_ = 0;
    int blackScore_ = 0;
};

}  // namespace kungfu
