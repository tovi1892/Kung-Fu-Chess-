#include <catch2/catch.hpp>
#include <memory>
#include <vector>
#include "engine/GameEngine.hpp"
#include "model/Board.hpp"
#include "model/GameConfig.hpp"
#include "model/Position.hpp"
#include "model/pieces/Rook.hpp"
#include "model/pieces/Knight.hpp"
#include "model/pieces/Pawn.hpp"
#include "model/pieces/Piece.hpp"
#include "TestHelpers.hpp"

using namespace kungfu;

TEST_CASE("Real-Time Collision Rules", "[collision][realtime]") {

    SECTION("Two enemy pieces converging mid-flight: whichever one enters the occupied square wins") {
        // Two rooks converge on (0,2), which is neither one's own final
        // destination. White (starting first) reaches (0,2) at its 2nd step
        // and just sits there mid-slide; Black - started kMsPerCell/2 later -
        // reaches (0,2) partway through White's residency there, so it's
        // Black's step that discovers the square occupied. Black wins and
        // stops right there, even though White started moving first.
        auto board = emptyBoardWithKings();
        GameEngine game(board);
        game.start();

        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Rook>(PlayerColor::Black, Position(0, 4)), Position(0, 4));

        REQUIRE(game.requestMove(Position(0, 0), Position(0, 4)).is_accepted);
        game.wait(GameConfig::kMsPerCell / 2);
        REQUIRE(game.requestMove(Position(0, 4), Position(0, 0)).is_accepted);
        game.wait(2 * GameConfig::kMsPerCell);  // the collision resolves

        auto pieceAt = board->pieceAt(Position(0, 2));
        REQUIRE(pieceAt.has_value());
        CHECK((*pieceAt)->color() == PlayerColor::Black);
        CHECK((*pieceAt)->state() == PieceState::Cooldown);

        bool whiteRookAlive = false;
        for (Piece* p : board->pieces()) {
            if (p->color() == PlayerColor::White && p->type() == PieceType::Rook) {
                whiteRookAlive = true;
            }
        }
        CHECK(whiteRookAlive == false);

        // The winner stops right where it won, rather than continuing on
        // toward (0,0) - its own original destination - once the cooldown
        // it just entered fully elapses too.
        game.wait(GameConfig::kBaseCooldownMs);
        auto stillAt = board->pieceAt(Position(0, 2));
        REQUIRE(stillAt.has_value());
        CHECK((*stillAt)->color() == PlayerColor::Black);
        CHECK((*stillAt)->state() == PieceState::Idle);
    }

    SECTION("Two friendly pieces converging mid-flight: the entering piece stops in place") {
        // Both rooks start on empty squares and race toward each other along
        // row 0, so the "friendly collision" only becomes real mid-flight
        // (via RealTimeArbiter's dynamic-collision check) rather than being
        // rejected upfront by RuleEngine's friendly_destination guard.
        auto board = emptyBoardWithKings();
        GameEngine game(board);
        game.start();

        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 6)), Position(0, 6));

        REQUIRE(game.requestMove(Position(0, 0), Position(0, 5)).is_accepted);
        REQUIRE(game.requestMove(Position(0, 6), Position(0, 1)).is_accepted);
        game.wait(5000);

        // The rook moving right (processed first each tick) claims (0,3) at
        // the collision instant, so the rook moving left stops one square
        // short, at (0,4). The winning rook then immediately runs into that
        // now-stationary friendly piece and also stops, at (0,3).
        auto winner = board->pieceAt(Position(0, 3));
        REQUIRE(winner.has_value());
        CHECK((*winner)->state() == PieceState::Idle);

        auto stopped = board->pieceAt(Position(0, 4));
        REQUIRE(stopped.has_value());
        CHECK((*stopped)->state() == PieceState::Idle);
    }

    SECTION("Knight jumps over pieces and captures only on landing") {
        auto board = emptyBoardWithKings();
        GameEngine game(board);
        game.start();

        board->placePiece(std::make_unique<Knight>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        // Sits "in the way" of the knight's L-shape; a knight must never be
        // blocked by it, friendly or not, and must never capture it either.
        board->placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(1, 0)), Position(1, 0));
        board->placePiece(std::make_unique<Pawn>(PlayerColor::Black, Position(2, 1)), Position(2, 1));

        REQUIRE(game.requestMove(Position(0, 0), Position(2, 1)).is_accepted);
        game.wait(2000);

        auto jumpedOver = board->pieceAt(Position(1, 0));
        REQUIRE(jumpedOver.has_value());
        CHECK((*jumpedOver)->type() == PieceType::Pawn);
        CHECK((*jumpedOver)->color() == PlayerColor::White);

        auto dest = board->pieceAt(Position(2, 1));
        REQUIRE(dest.has_value());
        CHECK((*dest)->type() == PieceType::Knight);

        REQUIRE(board->pieceAt(Position(0, 0)).has_value() == false);
    }

    SECTION("Knight cannot land on a friendly-occupied square") {
        auto board = emptyBoardWithKings();
        GameEngine game(board);
        game.start();

        board->placePiece(std::make_unique<Knight>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(2, 1)), Position(2, 1));

        const auto result = game.requestMove(Position(0, 0), Position(2, 1));
        REQUIRE(result.is_accepted == false);
        REQUIRE(result.reason == "friendly_destination");
    }

    SECTION("Capturing a static enemy mid-slide stops the move there, like standard chess") {
        auto board = emptyBoardWithKings();
        GameEngine game(board);
        game.start();

        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Pawn>(PlayerColor::Black, Position(0, 3)), Position(0, 3));

        // Requesting a move past the enemy pawn is now legal to *request*...
        const auto result = game.requestMove(Position(0, 0), Position(0, 6));
        REQUIRE(result.is_accepted);
        game.wait(6000);  // more than enough time to have reached (0,6) had it not stopped

        // ...but capturing the pawn along the way ends the move right there.
        auto stoppedAt = board->pieceAt(Position(0, 3));
        REQUIRE(stoppedAt.has_value());
        CHECK((*stoppedAt)->type() == PieceType::Rook);
        CHECK((*stoppedAt)->color() == PlayerColor::White);

        REQUIRE(board->pieceAt(Position(0, 6)).has_value() == false);
    }

    SECTION("A piece's origin square becomes a legal target the instant it starts moving") {
        auto board = emptyBoardWithKings();
        GameEngine game(board);
        game.start();

        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(4, 4)), Position(4, 4));
        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(4, 1)), Position(4, 1));

        // The rook at (4,4) starts leaving - still physically there (Board
        // hasn't stepped it into its next cell yet), but its state is
        // already Moving.
        REQUIRE(game.requestMove(Position(4, 4), Position(4, 7)).is_accepted);
        auto leaving = board->pieceAt(Position(4, 4));
        REQUIRE(leaving.has_value());
        CHECK((*leaving)->state() == PieceState::Moving);

        // The other rook can target (4,4) immediately - no need to wait for
        // the first rook to actually finish crossing into its next cell.
        const auto result = game.requestMove(Position(4, 1), Position(4, 4));
        CHECK(result.is_accepted);
        CHECK(result.reason == "ok");
    }

    SECTION("A friendly piece on cooldown still blocks its square (only Moving is exempt)") {
        auto board = emptyBoardWithKings();
        GameEngine game(board);
        game.start();

        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 3)), Position(0, 3));

        REQUIRE(game.requestMove(Position(0, 0), Position(0, 1)).is_accepted);
        game.wait(GameConfig::kMsPerCell);  // arrival - now on cooldown at (0,1)
        auto cooling = board->pieceAt(Position(0, 1));
        REQUIRE(cooling.has_value());
        CHECK((*cooling)->state() == PieceState::Cooldown);

        const auto result = game.requestMove(Position(0, 3), Position(0, 1));
        CHECK(result.is_accepted == false);
        CHECK(result.reason == "friendly_destination");
    }
}
