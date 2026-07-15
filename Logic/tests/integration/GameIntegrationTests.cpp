#include <cassert>
#include <memory>

#include "model/Board.hpp"
#include "engine/GameEngine.hpp"
#include "game/GameController.hpp"
#include "game/UIInputAdapter.hpp"
#include "model/pieces/Rook.hpp"

int main() {
    auto board = std::make_shared<kungfu::Board>();
    auto rook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    board->placePiece(std::move(rook), kungfu::Position(0, 0));

    auto game = std::make_shared<kungfu::GameEngine>(board, nullptr);
    game->start();
    auto controller = std::make_shared<kungfu::GameController>(game);
    auto adapter = std::make_shared<kungfu::UIInputAdapter>(
        [controller]() -> kungfu::IGameInputTarget& { return *controller; });

    adapter->handleClick(50, 50);
    adapter->handleClick(150, 50);
    game->wait(1000);

    assert(!board->pieceAt(kungfu::Position(0, 0)).has_value());
    assert(board->pieceAt(kungfu::Position(0, 1)).has_value());
    assert(board->pieceAt(kungfu::Position(0, 1)).value()->type() == kungfu::PieceType::Rook);

    return 0;
}
