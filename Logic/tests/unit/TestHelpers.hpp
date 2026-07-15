#pragma once

#include <memory>

#include "model/Board.hpp"
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

}  // namespace kungfu
