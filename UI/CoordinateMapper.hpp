#pragma once

#include "common/Position.hpp"

namespace kungfu {

// Converts between pixel coordinates on the rendered board and logical board cells.
class CoordinateMapper {
public:
    CoordinateMapper(int windowWidth, int windowHeight, int rows, int cols)
        : windowWidth_(windowWidth), windowHeight_(windowHeight), rows_(rows), cols_(cols) {}

    Position pixelToCell(int x, int y) const {
        const int col = clamp(x / cellWidth(), 0, cols_ - 1);
        const int row = clamp(y / cellHeight(), 0, rows_ - 1);
        return Position(row, col);
    }

    int cellTopLeftX(int col) const { return col * cellWidth(); }
    int cellTopLeftY(int row) const { return row * cellHeight(); }

    int cellWidth() const { return windowWidth_ / cols_; }
    int cellHeight() const { return windowHeight_ / rows_; }

private:
    static int clamp(int value, int lo, int hi) {
        if (value < lo) return lo;
        if (value > hi) return hi;
        return value;
    }

    int windowWidth_;
    int windowHeight_;
    int rows_;
    int cols_;
};

}  // namespace kungfu
