#pragma once

#include <memory>

#include "model/Board.hpp"
#include "model/pieces/King.hpp"
#include "engine/GameEngine.hpp"
#include "input/Controller.hpp"

namespace kungfu {

// Callers that need to simulate pixel/cell clicks should wrap the returned
// GameEngine in a Controller and call Controller::handleClick/handleCellClick.
inline std::shared_ptr<GameEngine> createGameWithAdapter(std::shared_ptr<Board> board) {
    auto game = std::make_shared<GameEngine>(std::move(board), nullptr);
    return game;
}

inline std::shared_ptr<GameEngine> createTestGame() {
    return createGameWithAdapter(std::make_shared<Board>());
}

// A blank board for tests that place their own scenario-specific pieces
// from scratch. Board's constructor never populates pieces_, so this is
// just std::make_shared<Board>() - named so call sites read as a deliberate
// choice ("start blank") rather than relying on that construction detail.
inline std::shared_ptr<Board> emptyBoard() {
    return std::make_shared<Board>();
}

// The same, plus a White/Black king tucked in the corner - the minimal
// setup most real-time/collision tests build their specific scenario on
// top of (a king is required so a captured-king game-over never triggers
// on the piece the test actually cares about).
inline std::shared_ptr<Board> emptyBoardWithKings() {
    auto board = emptyBoard();
    board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
    board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));
    return board;
}

}  // namespace kungfu
