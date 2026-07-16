#include <catch2/catch.hpp>
#include <memory>

#include "engine/GameEngine.hpp"
#include "model/Board.hpp"
#include "model/GameConfig.hpp"
#include "model/pieces/King.hpp"
#include "model/pieces/Rook.hpp"

using namespace kungfu;

namespace {

std::shared_ptr<Board> emptyBoardWithKings() {
    auto board = std::make_shared<Board>();
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            board->removePiece(Position(r, c));
        }
    }
    board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
    board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));
    return board;
}

}  // namespace

TEST_CASE("A piece enters cooldown on arrival and returns to Idle once it elapses", "[cooldown]") {
    auto board = emptyBoardWithKings();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board);
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 1)).is_accepted);
    game.wait(GameConfig::kMsPerCell);  // distance 1 - arrival

    auto arrived = board->pieceAt(Position(0, 1));
    REQUIRE(arrived.has_value());
    CHECK((*arrived)->state() == PieceState::Cooldown);

    game.wait(GameConfig::kBaseCooldownMs - 1);  // not quite there yet
    CHECK((*arrived)->state() == PieceState::Cooldown);

    game.wait(1);  // the last millisecond of cooldown
    CHECK((*arrived)->state() == PieceState::Idle);
}

TEST_CASE("Requesting a move for a piece on cooldown queues a premove instead of moving immediately", "[cooldown][premove]") {
    auto board = emptyBoardWithKings();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board);
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 1)).is_accepted);
    game.wait(GameConfig::kMsPerCell);  // arrival - now on cooldown

    const auto queued = game.requestMove(Position(0, 1), Position(0, 2));
    REQUIRE(queued.is_accepted);
    REQUIRE(queued.reason == "premove_queued");

    // Nothing has actually moved yet - the piece is still on cooldown at (0,1).
    auto stillAtOne = board->pieceAt(Position(0, 1));
    REQUIRE(stillAtOne.has_value());
    CHECK((*stillAtOne)->state() == PieceState::Cooldown);
    REQUIRE(board->pieceAt(Position(0, 2)).has_value() == false);
}

TEST_CASE("A premove fires automatically the instant cooldown ends", "[cooldown][premove]") {
    auto board = emptyBoardWithKings();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board);
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 1)).is_accepted);
    game.wait(GameConfig::kMsPerCell);  // arrival at (0,1) - cooldown starts

    REQUIRE(game.requestMove(Position(0, 1), Position(0, 2)).is_accepted);

    game.wait(GameConfig::kBaseCooldownMs);  // cooldown elapses; the queued premove should start now
    auto midFlight = board->pieceAt(Position(0, 1));
    REQUIRE(midFlight.has_value());
    CHECK((*midFlight)->state() == PieceState::Moving);

    game.wait(GameConfig::kMsPerCell);  // the premove's own travel time (distance 1)
    REQUIRE(board->pieceAt(Position(0, 1)).has_value() == false);
    auto finalPos = board->pieceAt(Position(0, 2));
    REQUIRE(finalPos.has_value());
    CHECK((*finalPos)->type() == PieceType::Rook);
}

TEST_CASE("A later illegal request cancels a queued premove", "[cooldown][premove]") {
    auto board = emptyBoardWithKings();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board);
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 1)).is_accepted);
    game.wait(GameConfig::kMsPerCell);  // arrival - cooldown starts

    REQUIRE(game.requestMove(Position(0, 1), Position(0, 2)).is_accepted);  // legitimate premove
    // Overwrite it with a request that will fail validation once cooldown
    // ends (not a straight/diagonal line for a rook) - this is the
    // documented way to cancel a premove.
    REQUIRE(game.requestMove(Position(0, 1), Position(5, 5)).is_accepted);  // still just "queued", not validated yet

    game.wait(GameConfig::kBaseCooldownMs);   // cooldown ends - the (now-overwritten) premove is validated and discarded
    game.wait(2 * GameConfig::kMsPerCell);    // plenty of time for a move to have happened, if one had started

    auto stillThere = board->pieceAt(Position(0, 1));
    REQUIRE(stillThere.has_value());
    CHECK((*stillThere)->type() == PieceType::Rook);
    CHECK((*stillThere)->state() == PieceState::Idle);
    REQUIRE(board->pieceAt(Position(0, 2)).has_value() == false);
    REQUIRE(board->pieceAt(Position(5, 5)).has_value() == false);
}

TEST_CASE("Game speed scales both movement time and cooldown duration", "[cooldown][speed]") {
    auto board = emptyBoardWithKings();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board, nullptr, /*speedMultiplier=*/2.0);
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 1)).is_accepted);
    game.wait(GameConfig::kMsPerCell / 2);  // half of the default time per cell

    auto arrived = board->pieceAt(Position(0, 1));
    REQUIRE(arrived.has_value());
    CHECK((*arrived)->state() == PieceState::Cooldown);

    game.wait(GameConfig::kBaseCooldownMs / 2);  // half of the default cooldown
    CHECK((*arrived)->state() == PieceState::Idle);
}
