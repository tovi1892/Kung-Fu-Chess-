#include <catch2/catch.hpp>
#include <memory>

#include "engine/GameEngine.hpp"
#include "history/GameRecord.hpp"
#include "model/Board.hpp"
#include "model/GameConfig.hpp"
#include "model/pieces/King.hpp"
#include "model/pieces/Knight.hpp"
#include "model/pieces/Pawn.hpp"
#include "model/pieces/Queen.hpp"
#include "model/pieces/Rook.hpp"

using namespace kungfu;

TEST_CASE("pieceValue matches the scoring table", "[history]") {
    CHECK(pieceValue(PieceType::Knight) == 3);
    CHECK(pieceValue(PieceType::Bishop) == 3);
    CHECK(pieceValue(PieceType::Rook) == 5);
    CHECK(pieceValue(PieceType::Queen) == 9);
    CHECK(pieceValue(PieceType::Pawn) == 1);
    CHECK(pieceValue(PieceType::King) == 0);
}

TEST_CASE("moveNotation builds short algebraic strings", "[history]") {
    // Pawn move: destination square only.
    CHECK(moveNotation(PieceType::Pawn, Position(1, 4), Position(3, 4), false) == "e5");

    // Pawn capture: origin file + x + destination.
    CHECK(moveNotation(PieceType::Pawn, Position(3, 4), Position(4, 3), true) == "exd4");

    // Non-pawn move: piece letter + destination.
    CHECK(moveNotation(PieceType::King, Position(0, 4), Position(1, 4), false) == "Ke7");
    CHECK(moveNotation(PieceType::Queen, Position(0, 3), Position(4, 3), false) == "Qd4");

    // Non-pawn capture: piece letter + x + destination.
    CHECK(moveNotation(PieceType::Knight, Position(0, 1), Position(2, 2), true) == "Nxc6");
}

TEST_CASE("GameRecord tracks moves and scores independently per color", "[history]") {
    GameRecord record;

    record.recordMove(PlayerColor::White, 100, "d4");
    record.recordMove(PlayerColor::Black, 250, "e5");
    record.recordMove(PlayerColor::White, 400, "Qd3");

    REQUIRE(record.movesFor(PlayerColor::White).size() == 2);
    CHECK(record.movesFor(PlayerColor::White)[0].notation == "d4");
    CHECK(record.movesFor(PlayerColor::White)[1].notation == "Qd3");
    REQUIRE(record.movesFor(PlayerColor::Black).size() == 1);
    CHECK(record.movesFor(PlayerColor::Black)[0].notation == "e5");

    CHECK(record.scoreFor(PlayerColor::White) == 0);
    CHECK(record.scoreFor(PlayerColor::Black) == 0);

    record.recordCapture(PlayerColor::White, PieceType::Pawn);
    record.recordCapture(PlayerColor::White, PieceType::Knight);
    record.recordCapture(PlayerColor::Black, PieceType::Queen);

    CHECK(record.scoreFor(PlayerColor::White) == 1 + 3);
    CHECK(record.scoreFor(PlayerColor::Black) == 9);
}

TEST_CASE("GameEngine records an accepted move's notation immediately", "[history][realtime]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board);
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 3)).is_accepted);

    REQUIRE(game.gameRecord().movesFor(PlayerColor::White).size() == 1);
    CHECK(game.gameRecord().movesFor(PlayerColor::White)[0].notation == "Rd8");
    CHECK(game.gameRecord().movesFor(PlayerColor::Black).empty());
}

TEST_CASE("A real-time capture updates the capturing color's score", "[history][realtime]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
    board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    board->placePiece(std::make_unique<Pawn>(PlayerColor::Black, Position(0, 3)), Position(0, 3));
    GameEngine game(board);
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 6)).is_accepted);
    game.wait(6 * GameConfig::kMsPerCell);  // plenty of time to reach the pawn and stop there

    CHECK(game.gameRecord().scoreFor(PlayerColor::White) == pieceValue(PieceType::Pawn));
    CHECK(game.gameRecord().scoreFor(PlayerColor::Black) == 0);
}

TEST_CASE("A knight capture on landing updates score too", "[history][realtime]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
    board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));
    board->placePiece(std::make_unique<Knight>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    board->placePiece(std::make_unique<Queen>(PlayerColor::Black, Position(2, 1)), Position(2, 1));
    GameEngine game(board);
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(2, 1)).is_accepted);
    game.wait(2 * GameConfig::kMsPerCell);

    CHECK(game.gameRecord().scoreFor(PlayerColor::White) == pieceValue(PieceType::Queen));
}
