#include <catch2/catch.hpp>
#include "pieces/King.hpp"
#include "pieces/Queen.hpp"
#include "pieces/Rook.hpp"
#include "pieces/Bishop.hpp"
#include "pieces/Knight.hpp"
#include "pieces/Pawn.hpp"

TEST_CASE("Piece type identification", "[piece_types]") {
    kungfu::King king(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    kungfu::Queen queen(kungfu::PlayerColor::Black, kungfu::Position(1, 1));
    kungfu::Rook rook(kungfu::PlayerColor::White, kungfu::Position(2, 2));
    kungfu::Bishop bishop(kungfu::PlayerColor::Black, kungfu::Position(3, 3));
    kungfu::Knight knight(kungfu::PlayerColor::White, kungfu::Position(4, 4));
    kungfu::Pawn pawn(kungfu::PlayerColor::Black, kungfu::Position(5, 5));

    REQUIRE(king.type() == kungfu::PieceType::King);
    REQUIRE(queen.type() == kungfu::PieceType::Queen);
    REQUIRE(rook.type() == kungfu::PieceType::Rook);
    REQUIRE(bishop.type() == kungfu::PieceType::Bishop);
    REQUIRE(knight.type() == kungfu::PieceType::Knight);
    REQUIRE(pawn.type() == kungfu::PieceType::Pawn);
}