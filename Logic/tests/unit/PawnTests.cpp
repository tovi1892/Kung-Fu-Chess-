#include <catch2/catch.hpp>
#include "model/Board.hpp"
#include "engine/GameEngine.hpp"
#include "model/pieces/Pawn.hpp"
#include "TestHelpers.hpp"

TEST_CASE("Pawn movement and promotion rules", "[pawn]") {
    auto board = std::make_shared<kungfu::Board>();
    board->placePiece(std::make_unique<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(1, 0)), kungfu::Position(1, 0));
    auto g = kungfu::createGameWithAdapter(board);
    g->start();

    SECTION("Double step from start is allowed") {
        REQUIRE(g->requestMove(kungfu::Position(1, 0), kungfu::Position(3, 0)).is_accepted == true);
    }

    SECTION("Promotion to queen on last row") {
        auto p2 = std::make_unique<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(6, 0));
        board->placePiece(std::move(p2), kungfu::Position(6, 0));
        g->requestMove(kungfu::Position(6, 0), kungfu::Position(7, 0));
        g->wait(1000);
        REQUIRE(board->pieceAt(kungfu::Position(7, 0)).value()->type() == kungfu::PieceType::Queen);
    }
}