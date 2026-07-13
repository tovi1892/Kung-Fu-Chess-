#include <catch2/catch.hpp>
#include "board/Board.hpp"
#include "game/Game.hpp"
#include "pieces/Rook.hpp"
#include "TestHelpers.hpp"

TEST_CASE("Jump mechanic logic", "[jump]") {
    auto board = std::make_shared<kungfu::Board>();
    board->placePiece(std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3)), kungfu::Position(3, 3));
    auto g = kungfu::createGameWithAdapter(board);
    g->start();

    SECTION("Try jump sets airborne state") {
        REQUIRE(g->tryJump(kungfu::Position(3, 3)) == true);
        REQUIRE(board->pieceAt(kungfu::Position(3, 3)).value()->isAirborne() == true);
    }

    SECTION("Resolve jump resets to idle") {
        g->tryJump(kungfu::Position(3, 3));
        g->resolveJump(kungfu::Position(3, 3));
        REQUIRE(board->pieceAt(kungfu::Position(3, 3)).value()->isAirborne() == false);
    }
}