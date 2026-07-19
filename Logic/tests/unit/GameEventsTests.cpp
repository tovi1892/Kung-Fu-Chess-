#include <catch2/catch.hpp>
#include <memory>
#include <optional>

#include "engine/GameEngine.hpp"
#include "model/GameConfig.hpp"
#include "model/pieces/King.hpp"
#include "model/pieces/Pawn.hpp"
#include "model/pieces/Rook.hpp"
#include "TestHelpers.hpp"

using namespace kungfu;

TEST_CASE("GameEngine::start publishes GameStarted", "[events][realtime]") {
    auto game = createTestGame();
    bool fired = false;
    game->onGameStarted().subscribe([&](const GameStarted&) { fired = true; });

    game->start();

    CHECK(fired);
}

TEST_CASE("An accepted move publishes MoveStarted with the right payload", "[events][realtime]") {
    auto board = emptyBoard();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    GameEngine game(board);

    std::optional<MoveStarted> captured;
    game.onMoveStarted().subscribe([&](const MoveStarted& e) { captured = e; });
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 3)).is_accepted);

    REQUIRE(captured.has_value());
    CHECK(captured->color == PlayerColor::White);
    CHECK(captured->type == PieceType::Rook);
    CHECK(captured->from == Position(0, 0));
    CHECK(captured->to == Position(0, 3));
    CHECK_FALSE(captured->isCapture);
    CHECK(captured->notation == "Rd8");
}

TEST_CASE("A real-time capture publishes PieceCaptured", "[events][realtime]") {
    auto board = emptyBoardWithKings();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    board->placePiece(std::make_unique<Pawn>(PlayerColor::Black, Position(0, 3)), Position(0, 3));
    GameEngine game(board);

    std::optional<PieceCaptured> captured;
    game.onPieceCaptured().subscribe([&](const PieceCaptured& e) { captured = e; });
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 6)).is_accepted);
    game.wait(6 * GameConfig::kMsPerCell);  // plenty of time to reach the pawn and stop there

    REQUIRE(captured.has_value());
    CHECK(captured->capturingColor == PlayerColor::White);
    CHECK(captured->capturedType == PieceType::Pawn);
    CHECK(captured->at == Position(0, 3));
}

TEST_CASE("Capturing the enemy king publishes GameEnded with the winner", "[events][realtime]") {
    auto board = emptyBoard();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
    board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(0, 3)), Position(0, 3));
    GameEngine game(board);

    std::optional<GameEnded> ended;
    game.onGameEnded().subscribe([&](const GameEnded& e) { ended = e; });
    game.start();

    REQUIRE(game.requestMove(Position(0, 0), Position(0, 6)).is_accepted);
    game.wait(6 * GameConfig::kMsPerCell);

    REQUIRE(ended.has_value());
    CHECK(ended->winner == PlayerColor::White);
    CHECK(game.isFinished());
}
