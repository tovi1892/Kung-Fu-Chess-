#include <catch2/catch.hpp>
#include <memory>
#include <vector>
#include "game/Game.hpp"
#include "board/Board.hpp"
#include "common/Position.hpp"
#include "pieces/Rook.hpp"
#include "pieces/Knight.hpp"
#include "pieces/King.hpp"
#include "pieces/Pawn.hpp"
#include "pieces/Queen.hpp"
#include "pieces/Bishop.hpp"
#include "pieces/Piece.hpp"

using namespace kungfu;

TEST_CASE("Real-Time Collision Rules", "[collision][realtime]") {

    SECTION("Rule 5: Dynamic Enemy Collision") {
        auto board = std::make_shared<Board>();
        Game game(board);
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

        REQUIRE(game.requestMove(Position(0, 0), Position(0, 4)));
        game.wait(500);
        REQUIRE(game.requestMove(Position(0, 4), Position(0, 0)));
        game.wait(2000);

        auto pieceAt = board->pieceAt(Position(0, 2));
        REQUIRE(pieceAt.has_value());
        CHECK((*pieceAt)->color() == PlayerColor::White);
    }

    SECTION("Rule 6: Dynamic Friendly Collision") {
        auto board = std::make_shared<Board>();
        Game game(board);
        game.start();

        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                board->removePiece(Position(r, c));
            }
        }

        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));
        board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 4)), Position(0, 4));
        board->placePiece(std::make_unique<King>(PlayerColor::White, Position(7, 7)), Position(7, 7));
        board->placePiece(std::make_unique<King>(PlayerColor::Black, Position(7, 6)), Position(7, 6));

        REQUIRE(game.requestMove(Position(0, 0), Position(0, 4)));
        game.wait(500);
        REQUIRE(game.requestMove(Position(0, 4), Position(0, 0)));
        game.wait(2000);

        auto pos = board->pieceAt(Position(0, 3));
        REQUIRE(pos.has_value());
        CHECK((*pos)->state() == PieceState::Idle);
    }

    SECTION("Rule 8: Knight Landing") {
        auto board = std::make_shared<Board>();
        Game game(board);
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

        REQUIRE(game.requestMove(Position(0, 0), Position(2, 1)));
        game.wait(2500);

        auto dest = board->pieceAt(Position(2, 1));
        REQUIRE(dest.has_value());
        CHECK((*dest)->type() == PieceType::Knight);
    }
}