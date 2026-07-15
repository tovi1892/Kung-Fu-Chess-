#include <catch2/catch.hpp>
#include "model/Board.hpp"
#include "model/GameConfig.hpp"
#include "engine/GameEngine.hpp"
#include "model/pieces/King.hpp"
#include "model/pieces/Knight.hpp"
#include "model/pieces/Rook.hpp"
#include "TestHelpers.hpp"

using namespace kungfu;

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

TEST_CASE("An airborne piece lands back to Idle on its own once kBaseAirborneMs elapses", "[jump][realtime]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(3, 3)), Position(3, 3));
    GameEngine game(board);
    game.start();

    REQUIRE(game.tryJump(Position(3, 3)));
    CHECK(board->pieceAt(Position(3, 3)).value()->isAirborne());

    game.wait(GameConfig::kBaseAirborneMs - 1);
    CHECK(board->pieceAt(Position(3, 3)).value()->isAirborne());

    game.wait(1);  // the last millisecond of the airborne window
    CHECK(board->pieceAt(Position(3, 3)).value()->state() == PieceState::Idle);
}

TEST_CASE("A piece cannot jump while already airborne, moving, or on cooldown", "[jump][realtime]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board);
    game.start();

    REQUIRE(game.tryJump(Position(0, 0)));
    CHECK(game.tryJump(Position(0, 0)) == false);  // already airborne

    game.wait(GameConfig::kBaseAirborneMs);  // lands back to Idle

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 1)).is_accepted);
    CHECK(game.tryJump(Position(0, 1)) == false);  // mid-move

    game.wait(GameConfig::kMsPerCell);  // arrival - now on cooldown
    CHECK(board->pieceAt(Position(0, 1)).value()->state() == PieceState::Cooldown);
    CHECK(game.tryJump(Position(0, 1)) == false);  // cooling down
}

TEST_CASE("Requesting a normal move for an airborne piece is rejected", "[jump][realtime]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board);
    game.start();

    REQUIRE(game.tryJump(Position(0, 0)));

    const auto result = game.requestMove(Position(0, 0), Position(0, 1));
    CHECK(result.is_accepted == false);
    CHECK(result.reason == "piece_airborne");
}

TEST_CASE("An enemy sliding piece that reaches an airborne square kills the attacker instead of the jumper", "[jump][realtime]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
    board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(3, 3)), Position(3, 3));
    board->placePiece(std::make_unique<Rook>(PlayerColor::Black, Position(3, 2)), Position(3, 2));
    GameEngine game(board);
    game.start();

    REQUIRE(game.tryJump(Position(3, 3)));
    REQUIRE(game.requestMove(Position(3, 2), Position(3, 3)).is_accepted);

    game.wait(GameConfig::kMsPerCell);  // distance 1 - arrival at the jumper's square

    CHECK(board->pieceAt(Position(3, 2)).has_value() == false);  // the attacker died mid-flight
    auto survivor = board->pieceAt(Position(3, 3));
    REQUIRE(survivor.has_value());
    CHECK((*survivor)->color() == PlayerColor::White);
    CHECK((*survivor)->state() == PieceState::Idle);  // landed, no longer airborne
}

TEST_CASE("A knight landing on an airborne enemy is also killed by the counter-kill", "[jump][realtime]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
    board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(3, 3)), Position(3, 3));
    board->placePiece(std::make_unique<Knight>(PlayerColor::Black, Position(1, 2)), Position(1, 2));
    GameEngine game(board);
    game.start();

    REQUIRE(game.tryJump(Position(3, 3)));
    REQUIRE(game.requestMove(Position(1, 2), Position(3, 3)).is_accepted);  // valid knight L-shape

    game.wait(2 * GameConfig::kMsPerCell);  // knight's fixed 2-cell hop timing

    CHECK(board->pieceAt(Position(1, 2)).has_value() == false);  // the knight died on landing
    auto survivor = board->pieceAt(Position(3, 3));
    REQUIRE(survivor.has_value());
    CHECK((*survivor)->type() == PieceType::Rook);
    CHECK((*survivor)->state() == PieceState::Idle);
}