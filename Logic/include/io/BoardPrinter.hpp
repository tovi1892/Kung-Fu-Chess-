#pragma once

#include <ostream>
#include <string>
#include "model/IBoard.hpp"

namespace kungfu {

// Text I/O: prints logical board occupancy (course spec section 15). Depends
// only on model data - not on GameEngine, Controller, RuleEngine, or
// RealTimeArbiter. Prints occupancy only, never animation/pixel position.
class BoardPrinter {
public:
    void print(std::ostream& out, const IBoard& board, int rows, int cols) const;

private:
    static std::string pieceToken(const Piece* piece);
};

}  // namespace kungfu
