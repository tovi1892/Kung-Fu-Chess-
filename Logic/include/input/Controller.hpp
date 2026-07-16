#pragma once

#include <memory>
#include <optional>

#include "model/Position.hpp"
#include "engine/GameEngine.hpp"

namespace kungfu {

// Click interpretation and selected-cell state. Owns the only selection
// state in the system; does not decide chess legality, mutate the Board, or
// render anything.
class Controller final {
public:
    explicit Controller(std::shared_ptr<GameEngine> game);

    // Swaps the engine this controller drives and clears any current selection.
    void attachGame(std::shared_ptr<GameEngine> game);
    std::shared_ptr<GameEngine> game() const;

    // Raw pixel click, mapped to a board cell using the fixed CELL_SIZE
    // convention (GameConfig::kCellSizePx), then handled the same as
    // handleCellClick.
    bool handleClick(int x, int y);

    // The actual selection/move/jump state machine, given an already-resolved
    // board cell: no selection yet selects a selectable piece; clicking the
    // same cell again requests a jump there; clicking another friendly piece
    // switches the selection to it; anything else is forwarded as a move
    // request. Returns whether the click resulted in an accepted action.
    bool handleCellClick(int row, int col);

    // Forwards to GameEngine::wait - what the render loop calls every frame
    // to advance real time.
    void handleTimePassed(int ms);

    bool isRunning() const;

    // True when a piece is currently selected and 'pos' holds a same-color piece.
    bool isFriendlyPieceAt(const Position& pos) const;

    // A direct, pixel-free equivalent of handleCellClick's selection logic.
    // Not called from anywhere in this codebase today (no UI path, no test)
    // - a parallel entry point, not currently exercised.
    bool selectPiece(const Position& pos);

    // Direct move/jump requests that bypass the click/selection flow
    // entirely, clearing any selection on success. Used by tests that don't
    // want to simulate clicks.
    bool requestMove(const Position& from, const Position& to);
    bool requestJump(const Position& pos);

    bool hasSelection() const;
    std::optional<Position> selectedPosition() const;

    // Delegate straight to the attached GameEngine.
    bool isPositionInBounds(const Position& pos) const;
    int boardRows() const;
    int boardCols() const;

private:
    std::shared_ptr<GameEngine> game_;
    std::optional<Position> selectedPosition_;
};

}  // namespace kungfu
