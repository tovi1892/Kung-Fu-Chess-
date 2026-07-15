#pragma once

#include <memory>
#include <optional>

#include "model/Position.hpp"
#include "engine/GameEngine.hpp"

namespace kungfu {

// Controller: click interpretation and selected-cell state (course spec
// section 11). Owns the only selection state in the system; does not decide
// chess legality, mutate the Board, or render anything.
class Controller final {
public:
    explicit Controller(std::shared_ptr<GameEngine> game);

    void attachGame(std::shared_ptr<GameEngine> game);
    std::shared_ptr<GameEngine> game() const;

    // Raw pixel click, mapped to a board cell using the fixed CELL_SIZE
    // convention (GameConfig::kCellSizePx), then handled the same as
    // handleCellClick.
    bool handleClick(int x, int y);

    bool handleCellClick(int row, int col);
    void handleTimePassed(int ms);

    bool isRunning() const;
    bool isFriendlyPieceAt(const Position& pos) const;
    bool selectPiece(const Position& pos);
    bool requestMove(const Position& from, const Position& to);
    bool requestJump(const Position& pos);
    bool hasSelection() const;
    std::optional<Position> selectedPosition() const;
    bool isPositionInBounds(const Position& pos) const;
    int boardRows() const;
    int boardCols() const;

private:
    std::shared_ptr<GameEngine> game_;
    std::optional<Position> selectedPosition_;
};

}  // namespace kungfu
