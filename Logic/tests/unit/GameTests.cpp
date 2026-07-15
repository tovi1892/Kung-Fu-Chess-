#include <catch2/catch.hpp>
#include "model/Board.hpp"
#include "game/Game.hpp"
#include "model/pieces/King.hpp"
#include "model/pieces/Rook.hpp"
#include "TestHelpers.hpp"

TEST_CASE("Game state and logic flow", "[game]") {
    auto game = kungfu::createTestGame();

    SECTION("Basic state machine") {
        REQUIRE(game->isRunning() == false);
        game->start();
        REQUIRE(game->isRunning() == true);
        game->stop();
        REQUIRE(game->isRunning() == false);
    }

    SECTION("Capturing king ends game") {
        auto board = std::make_shared<kungfu::Board>();
        board->placePiece(std::make_unique<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0)), kungfu::Position(0, 0));
        board->placePiece(std::make_unique<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(1, 1)), kungfu::Position(1, 1));

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        REQUIRE(g->tryMove(kungfu::Position(0, 0), kungfu::Position(1, 1)) == true);
        REQUIRE(g->isFinished() == true);
    }
}