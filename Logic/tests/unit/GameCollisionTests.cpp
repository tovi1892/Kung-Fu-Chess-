#include <catch2/catch.hpp>
#include <memory>
#include <vector>
#include "engine/GameEngine.hpp"
#include "model/Board.hpp"
#include "model/Position.hpp"
#include "model/pieces/Rook.hpp"
#include "model/pieces/Knight.hpp"
#include "model/pieces/King.hpp"
#include "model/pieces/Pawn.hpp"
#include "model/pieces/Queen.hpp"
#include "model/pieces/Bishop.hpp"
#include "model/pieces/Piece.hpp"

using namespace kungfu;

TEST_CASE("Real-Time Collision Rules", "[collision][realtime]") {

    SECTION("Rule 5: Dynamic Enemy Collision") {
        auto board = std::make_shared<Board>();
        GameEngine game(board);
        game.start();

        // ניקוי הלוח
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                board->removePiece(Position(r, c));
            }
        }

        // יצירת הכלים עם שני פרמטרים כנדרש בבנאי: (Color, Position)
        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Rook>(PlayerColor::Black, Position(0, 4)), Position(0, 4));
        board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
        board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));

        REQUIRE(game.requestMove(Position(0, 0), Position(0, 4)).is_accepted);
        game.wait(500);
        REQUIRE(game.requestMove(Position(0, 4), Position(0, 0)).is_accepted);
        game.wait(2000);

        auto pieceAt = board->pieceAt(Position(0, 2));
        REQUIRE(pieceAt.has_value());
        CHECK((*pieceAt)->color() == PlayerColor::White);
    }

    SECTION("Rule 6: Dynamic Friendly Collision") {
        // Both rooks start on empty squares and race toward each other along
        // row 0, so the "friendly collision" only becomes real mid-flight
        // (via RealTimeArbiter's dynamic-collision check) rather than being
        // rejected upfront by RuleEngine's friendly_destination guard.
        auto board = std::make_shared<Board>();
        GameEngine game(board);
        game.start();

        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                board->removePiece(Position(r, c));
            }
        }

        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 6)), Position(0, 6));
        board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
        board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));

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

    SECTION("Rule 8: Knight jumps over pieces and captures only on landing") {
        auto board = std::make_shared<Board>();
        GameEngine game(board);
        game.start();

        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                board->removePiece(Position(r, c));
            }
        }

        board->placePiece(std::make_unique<Knight>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        // Sits "in the way" of the knight's L-shape; a knight must never be
        // blocked by it, friendly or not, and must never capture it either.
        board->placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(1, 0)), Position(1, 0));
        board->placePiece(std::make_unique<Pawn>(PlayerColor::Black, Position(2, 1)), Position(2, 1));
        board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
        board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));

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

    SECTION("Rule 8b: Knight cannot land on a friendly-occupied square") {
        auto board = std::make_shared<Board>();
        GameEngine game(board);
        game.start();

        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                board->removePiece(Position(r, c));
            }
        }

        board->placePiece(std::make_unique<Knight>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(2, 1)), Position(2, 1));
        board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
        board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));

        const auto result = game.requestMove(Position(0, 0), Position(2, 1));
        REQUIRE(result.is_accepted == false);
        REQUIRE(result.reason == "friendly_destination");
    }
}