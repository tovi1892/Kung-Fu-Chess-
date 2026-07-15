#pragma once

#include <optional>
#include "model/Position.hpp"

namespace kungfu {

class MovementSystem {
public:
    bool isInBounds(const Position& position) const;
    bool isInBounds(const Position& position, int rows, int cols) const;
    bool isSamePosition(const Position& from, const Position& to) const;
    bool canMoveTo(const Position& from, const Position& to) const;

    std::optional<Position> pawnDoubleStepMiddle(const Position& from, const Position& to) const;
};

}  // namespace kungfu