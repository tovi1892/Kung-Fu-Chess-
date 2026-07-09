#pragma once

#include <optional>
#include "common/Position.hpp"
#include "pieces/Piece.hpp"

namespace kungfu {

class MovementSystem {
public:
    bool isInBounds(const Position& position) const;
    bool isInBounds(const Position& position, int rows, int cols) const; // פונקציה חדשה
    bool isSamePosition(const Position& from, const Position& to) const;
    bool canMoveTo(const Position& from, const Position& to) const;
    bool isValidMove(const Piece& piece, const Position& from, const Position& to) const;

    std::optional<Position> pawnDoubleStepMiddle(const Position& from, const Position& to) const;
};

}  // namespace kungfu

