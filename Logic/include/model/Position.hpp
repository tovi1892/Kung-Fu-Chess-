#pragma once

namespace kungfu {

// A single (row, col) board square. Pure geometry - no bounds checking, no
// awareness of what (if anything) occupies the square.
class Position {
public:
    // (0, 0).
    Position();
    Position(int row, int col);

    int row() const;
    int col() const;

    // other.row() - this.row().
    int rowDelta(const Position& other) const;
    // other.col() - this.col().
    int colDelta(const Position& other) const;

    // True when 'other' is a different square on the same diagonal (|Δrow| == |Δcol|).
    bool isDiagonalTo(const Position& other) const;
    // True when 'other' is a different square on the same row or column.
    bool isStraightTo(const Position& other) const;
    // True when 'other' is a knight's-move away ((2,1) or (1,2) shape).
    bool isKnightJumpTo(const Position& other) const;

    bool operator==(const Position& other) const;
    bool operator!=(const Position& other) const;
    // Orders by row, then col - what makes Position usable as a std::set/std::map key.
    bool operator<(const Position& other) const;

private:
    int row_;
    int col_;
};

}  // namespace kungfu
