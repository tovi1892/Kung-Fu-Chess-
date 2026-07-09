#include "common/Position.hpp"

namespace kungfu {

Position::Position() : row_(0), col_(0) {}

Position::Position(int row, int col) : row_(row), col_(col) {}

int Position::row() const {
    return row_;
}

int Position::col() const {
    return col_;
}

bool Position::operator==(const Position& other) const {
    return row_ == other.row_ && col_ == other.col_;
}

bool Position::operator!=(const Position& other) const {
    return !(*this == other);
}

}  // namespace kungfu
