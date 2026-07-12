#include <memory>

#include "board/Board.hpp"
#include "game/Game.hpp"
#include "game/UIInputAdapter.hpp"

int main() {
    auto board = std::make_shared<kungfu::Board>();
    std::shared_ptr<kungfu::Game> game;
    auto inputAdapter = std::make_shared<kungfu::UIInputAdapter>(
        [&game]() -> kungfu::IGameInputTarget& { return *game; });

    game = std::make_shared<kungfu::Game>(board, nullptr, inputAdapter);
    (void)game;
    return 0;
}
