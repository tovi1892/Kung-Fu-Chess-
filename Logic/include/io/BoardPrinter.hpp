#pragma once

#include <ostream>
#include <string>
#include "model/IBoard.hpp"

namespace kungfu {

// Text I/O: prints logical board occupancy. Depends only on model data - not
// on GameEngine, Controller, RuleEngine, or RealTimeArbiter. Prints
// occupancy only, never animation/pixel position.
class BoardPrinter {
public:
    // Writes one space-separated row per line ("wK", "bP", ... or "." for
    // empty) - the DSL's only assertion mechanism: a script's expected
    // output is just what this produces.
    void print(std::ostream& out, const IBoard& board, int rows, int cols) const;

private:
    static std::string pieceToken(const Piece* piece);
};

}  // namespace kungfu
