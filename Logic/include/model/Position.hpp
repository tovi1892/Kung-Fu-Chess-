#pragma once

namespace kungfu {

class Position {
public:
    Position();
    Position(int row, int col);

    int row() const;
    int col() const;

    int rowDelta(const Position& other) const;
    int colDelta(const Position& other) const;
    bool isDiagonalTo(const Position& other) const;
    bool isStraightTo(const Position& other) const;
    bool isKnightJumpTo(const Position& other) const;

    bool operator==(const Position& other) const;
    bool operator!=(const Position& other) const;

private:
    int row_;
    int col_;
};

}  // namespace kungfu
