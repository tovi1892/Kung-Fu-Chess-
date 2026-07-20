#pragma once

#include "model/Position.hpp"

namespace kungfu {

// Converts between pixel coordinates on the rendered board and logical board
// cells. The board grid is inset by `margin` pixels on every side (leaving
// room to draw a-h/1-8 coordinate labels around it); cellWidth/cellHeight are
// computed from the space that remains after removing the margins.
//
// boardWidth/boardHeight are the board's own square area, not necessarily
// the whole window - offsetX/offsetY say where that area starts within a
// larger window (e.g. after a side panel), so a single mapper stays the one
// source of truth for both drawing (OpenCvView) and click handling
// (main.cpp's BoardClickHandler) without the two ever drifting out of sync.
class CoordinateMapper {
public:
    static constexpr int kDefaultMargin = 40;

    CoordinateMapper(int boardWidth, int boardHeight, int rows, int cols,
                      int margin = kDefaultMargin, int offsetX = 0, int offsetY = 0)
        : boardWidth_(boardWidth), boardHeight_(boardHeight), rows_(rows), cols_(cols),
          margin_(margin), offsetX_(offsetX), offsetY_(offsetY) {}

    // Returns the cell a pixel falls in. The row/col may be outside
    // [0,rows)/[0,cols) when the click landed in the margin (labels), in a
    // side panel, or outside the window entirely - callers should treat
    // that as "outside the board" (GameEngine::isPositionInBounds already does).
    Position pixelToCell(int x, int y) const {
        const int col = (x - offsetX_ - margin_) / cellWidth();
        const int row = (y - offsetY_ - margin_) / cellHeight();
        return Position(row, col);
    }

    int cellTopLeftX(int col) const { return offsetX_ + margin_ + col * cellWidth(); }
    int cellTopLeftY(int row) const { return offsetY_ + margin_ + row * cellHeight(); }

    // Fractional versions of the above, for a piece mid-move: GameEngine
    // already hands out a fractional row/col (linear interpolation between
    // from and to), so rounding to a whole cell before mapping to pixels
    // would throw that away and make the piece visibly jump once mid-flight
    // instead of sliding.
    double cellTopLeftXf(double col) const { return offsetX_ + margin_ + col * cellWidth(); }
    double cellTopLeftYf(double row) const { return offsetY_ + margin_ + row * cellHeight(); }

    int cellWidth() const { return (boardWidth_ - 2 * margin_) / cols_; }
    int cellHeight() const { return (boardHeight_ - 2 * margin_) / rows_; }
    int margin() const { return margin_; }
    int offsetX() const { return offsetX_; }
    int offsetY() const { return offsetY_; }
    int boardWidth() const { return boardWidth_; }
    int boardHeight() const { return boardHeight_; }
    int rows() const { return rows_; }
    int cols() const { return cols_; }

private:
    int boardWidth_;
    int boardHeight_;
    int rows_;
    int cols_;
    int margin_;
    int offsetX_;
    int offsetY_;
};

}  // namespace kungfu
