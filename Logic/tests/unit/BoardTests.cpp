#include <catch2/catch.hpp>
#include <memory>
#include "board/Board.hpp"
#include "pieces/King.hpp"

TEST_CASE("Board operations - basic piece management", "[board]") {
    kungfu::Board board;
    
    // יצירת מלך לבן
    auto king = std::make_unique<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0));

    SECTION("Placing a piece on an empty square succeeds") {
        REQUIRE(board.placePiece(std::move(king), kungfu::Position(0, 0)) == true);
        REQUIRE(board.pieceAt(kungfu::Position(0, 0)).has_value() == true);
    }

    SECTION("Placing a piece on an occupied square fails") {
        board.placePiece(std::move(king), kungfu::Position(0, 0));
        
        auto otherKing = std::make_unique<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(1, 1));
        REQUIRE(board.placePiece(std::move(otherKing), kungfu::Position(0, 0)) == false);
    }

    SECTION("Moving a piece updates its position correctly") {
        board.placePiece(std::move(king), kungfu::Position(0, 0));
        
        REQUIRE(board.movePiece(kungfu::Position(0, 0), kungfu::Position(1, 1)) == true);
        REQUIRE(board.pieceAt(kungfu::Position(0, 0)).has_value() == false);
        REQUIRE(board.pieceAt(kungfu::Position(1, 1)).has_value() == true);
    }

    SECTION("Removing a piece works and handles invalid removals") {
        board.placePiece(std::move(king), kungfu::Position(0, 0));
        board.movePiece(kungfu::Position(0, 0), kungfu::Position(1, 1));
        
        // הסרה תקינה
        REQUIRE(board.removePiece(kungfu::Position(1, 1)) == true);
        REQUIRE(board.pieceAt(kungfu::Position(1, 1)).has_value() == false);
        
        // הסרה נוספת מאותו מקום צריכה להיכשל
        REQUIRE(board.removePiece(kungfu::Position(1, 1)) == false);
    }
}