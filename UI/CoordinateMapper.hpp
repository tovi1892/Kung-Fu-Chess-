#pragma once

#include "model/Position.hpp"

namespace kungfu {

// Converts between pixel coordinates on the rendered board and logical board
// cells. The board grid is inset by `margin` pixels on every side (leaving
// room to draw a-h/1-8 coordinate labels around it); cellWidth/cellHeight are
// computed from the space that remains after removing the margins.
class CoordinateMapper {
public:
    static constexpr int kDefaultMargin = 40;

    CoordinateMapper(int windowWidth, int windowHeight, int rows, int cols, int margin = kDefaultMargin)
        : windowWidth_(windowWidth), windowHeight_(windowHeight), rows_(rows), cols_(cols), margin_(margin) {}

    // Returns the cell a pixel falls in. The row/col may be outside
    // [0,rows)/[0,cols) when the click landed in the margin (labels) or
    // outside the window entirely - callers should treat that as "outside
    // the board" (GameEngine::isPositionInBounds already does).
    Position pixelToCell(int x, int y) const {
        const int col = (x - margin_) / cellWidth();
        const int row = (y - margin_) / cellHeight();
        return Position(row, col);
    }

    int cellTopLeftX(int col) const { return margin_ + col * cellWidth(); }
    int cellTopLeftY(int row) const { return margin_ + row * cellHeight(); }

    int cellWidth() const { return (windowWidth_ - 2 * margin_) / cols_; }
    int cellHeight() const { return (windowHeight_ - 2 * margin_) / rows_; }
    int margin() const { return margin_; }

private:
    int windowWidth_;
    int windowHeight_;
    int rows_;
    int cols_;
    int margin_;
};

}  // namespace kungfu
