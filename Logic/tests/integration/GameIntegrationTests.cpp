#include <cassert>
#include <memory>

#include "board/Board.hpp"
#include "game/Game.hpp"
#include "game/UIInputAdapter.hpp"
#include "pieces/Rook.hpp"

int main() {
    auto board = std::make_shared<kungfu::Board>();
    auto rook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    board->placePiece(std::move(rook), kungfu::Position(0, 0));

    std::shared_ptr<kungfu::Game> game;
    auto adapter = std::make_shared<kungfu::UIInputAdapter>(
        [&game]() -> kungfu::IGameInputTarget& { return *game; });

    game = std::make_shared<kungfu::Game>(board, nullptr, adapter);
    game->start();

    game->click(50, 50);
    game->click(150, 50);
    game->wait(1000);

    assert(!board->pieceAt(kungfu::Position(0, 0)).has_value());
    assert(board->pieceAt(kungfu::Position(0, 1)).has_value());
    assert(board->pieceAt(kungfu::Position(0, 1)).value()->type() == kungfu::PieceType::Rook);

    return 0;
}
