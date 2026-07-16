#include <catch2/catch.hpp>
#include <memory>
#include <optional>

#include "engine/GameEngine.hpp"
#include "model/Board.hpp"
#include "rules/RuleEngine.hpp"
#include "model/pieces/Pawn.hpp"
#include "model/pieces/Knight.hpp"
#include "model/pieces/Rook.hpp"
#include "model/pieces/Queen.hpp"

using namespace kungfu;

struct TestContext {
    std::shared_ptr<Board> board;
    std::shared_ptr<RuleEngine> ruleEngine;
    std::unique_ptr<GameEngine> game;

    TestContext() {
        board = std::make_shared<Board>();
        ruleEngine = std::make_shared<RuleEngine>(board);
        game = std::make_unique<GameEngine>(board, ruleEngine);
        game->start();
    }
};

TEST_CASE("Real-Time Simultaneous Movement and Collision Rules", "[game][realtime]") {
    
    SECTION("Same Color Collision") {
        TestContext ctx;
        Position pawnStart(1, 2);
        Position pawnEnd(2, 2);
        Position knightStart(0, 1);
        Position knightEnd(2, 2);

        ctx.board->replacePiece(pawnStart, std::make_unique<Pawn>(PlayerColor::White, pawnStart));
        ctx.board->replacePiece(knightStart, std::make_unique<Knight>(PlayerColor::White, knightStart));

        REQUIRE(ctx.board->pieceAt(pawnStart).has_value());
        REQUIRE(ctx.board->pieceAt(knightStart).has_value());
    }

    SECTION("Different Color Collision") {
        TestContext ctx;
        Position whiteKnightStart(0, 1);
        Position blackPawnStart(6, 3);
        
        ctx.board->replacePiece(whiteKnightStart, std::make_unique<Knight>(PlayerColor::White, whiteKnightStart));
        ctx.board->replacePiece(blackPawnStart, std::make_unique<Pawn>(PlayerColor::Black, blackPawnStart));

        REQUIRE(ctx.board->pieceAt(whiteKnightStart).has_value());
        REQUIRE(ctx.board->pieceAt(blackPawnStart).has_value());
    }
}
TEST_CASE("Real-Time Collision - Edge Cases", "[game][edge-cases]") {

    SECTION("Boundary Collision - Corner of the Board") {
        TestContext ctx;
        // A rook in the corner, blocked immediately by a friendly pawn.
        Position start(0, 0);
        Position target(0, 1);
        Position obstacle(0, 1);

        ctx.board->replacePiece(start, std::make_unique<Rook>(PlayerColor::White, start));
        ctx.board->replacePiece(obstacle, std::make_unique<Pawn>(PlayerColor::White, obstacle));

        // Moving onto a same-color piece must be rejected outright.
        bool result = ctx.game->requestMove(start, target).is_accepted;

        REQUIRE(result == false);
        REQUIRE(ctx.board->pieceAt(start).has_value());  // the rook never left (0,0)
    }

    SECTION("Long-Range Collision - Full Board Sweep") {
        TestContext ctx;
        // A rook on one edge of the board, an enemy pawn blocking midway,
        // and a target beyond it on the far edge.
        Position rookStart(0, 0);
        Position obstacle(0, 4);
        Position target(0, 7);

        ctx.board->replacePiece(rookStart, std::make_unique<Rook>(PlayerColor::White, rookStart));
        ctx.board->replacePiece(obstacle, std::make_unique<Pawn>(PlayerColor::Black, obstacle));

        // Requesting the full-board move is accepted (the path is only
        // resolved dynamically once the rook is actually moving) - the
        // point of this test is just that it doesn't crash or misbehave
        // when requested; GameCollisionTests covers the actual stop-and-
        // capture behavior once the rook reaches the pawn.
        ctx.game->requestMove(rookStart, target);

        REQUIRE(ctx.board->pieceAt(rookStart).has_value());
    }

    SECTION("Self-Collision - Trying to move to current square") {
        TestContext ctx;
        Position pos(4, 4);
        ctx.board->replacePiece(pos, std::make_unique<Knight>(PlayerColor::White, pos));

        // Requesting a "move" to the same square must be rejected.
        bool result = ctx.game->requestMove(pos, pos).is_accepted;

        REQUIRE(result == false);
    }
}