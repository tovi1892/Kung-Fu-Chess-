#include "common/Position.hpp"
#include <cmath>

namespace kungfu {

Position::Position() : row_(0), col_(0) {}

Position::Position(int row, int col) : row_(row), col_(col) {}

int Position::row() const {
    return row_;
}

int Position::col() const {
    return col_;
}

int Position::rowDelta(const Position& other) const {
    return other.row_ - row_;
}

int Position::colDelta(const Position& other) const {
    return other.col_ - col_;
}

bool Position::isDiagonalTo(const Position& other) const {
    const int deltaRow = std::abs(rowDelta(other));
    const int deltaCol = std::abs(colDelta(other));
    return deltaRow == deltaCol && deltaRow != 0;
}

bool Position::isStraightTo(const Position& other) const {
    return (row_ == other.row_ || col_ == other.col_) && !(*this == other);
}

bool Position::isKnightJumpTo(const Position& other) const {
    const int deltaRow = std::abs(rowDelta(other));
    const int deltaCol = std::abs(colDelta(other));
    return (deltaRow == 2 && deltaCol == 1) || (deltaRow == 1 && deltaCol == 2);
}

bool Position::operator==(const Position& other) const {
    return row_ == other.row_ && col_ == other.col_;
}

bool Position::operator!=(const Position& other) const {
    return !(*this == other);
}

}  // namespace kungfu
