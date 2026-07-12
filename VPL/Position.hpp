#pragma once

namespace kungfu {

class Position {
public:
    Position();
    Position(int row, int col);

    int row() const;
    int col() const;

    bool operator==(const Position& other) const;
    bool operator!=(const Position& other) const;

private:
    int row_;
    int col_;
};

}  // namespace kungfu
