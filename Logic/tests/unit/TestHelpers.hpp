#pragma once

#include <memory>

#include "model/Board.hpp"
#include "engine/GameEngine.hpp"
#include "game/GameController.hpp"
#include "game/UIInputAdapter.hpp"

namespace kungfu {

// Callers that need to simulate pixel clicks should wrap the returned GameEngine in
// a GameController and bind a UIInputAdapter to that controller - GameEngine itself
// no longer implements IGameInputTarget (that's Controller's responsibility).
inline std::shared_ptr<GameEngine> createGameWithAdapter(std::shared_ptr<Board> board) {
    auto game = std::make_shared<GameEngine>(std::move(board), nullptr);
    return game;
}

inline std::shared_ptr<GameEngine> createTestGame() {
    return createGameWithAdapter(std::make_shared<Board>());
}

}  // namespace kungfu
