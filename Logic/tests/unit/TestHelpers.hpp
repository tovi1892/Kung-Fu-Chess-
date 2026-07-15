#pragma once

#include <memory>

#include "model/Board.hpp"
#include "game/Game.hpp"
#include "game/UIInputAdapter.hpp"

namespace kungfu {

inline std::shared_ptr<Game> createGameWithAdapter(std::shared_ptr<Board> board) {
    auto gameHolder = std::make_shared<std::shared_ptr<Game>>();
    auto adapter = std::make_shared<UIInputAdapter>(
        [gameHolder]() -> IGameInputTarget& { return **gameHolder; });
    auto game = std::make_shared<Game>(std::move(board), nullptr, adapter);
    *gameHolder = game;
    return game;
}

inline std::shared_ptr<Game> createTestGame() {
    return createGameWithAdapter(std::make_shared<Board>());
}

}  // namespace kungfu
