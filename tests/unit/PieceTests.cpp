#include <catch2/catch.hpp>
#include "pieces/King.hpp"

TEST_CASE("Piece basic properties", "[piece]") {
    kungfu::King piece(kungfu::PlayerColor::White, kungfu::Position(0, 0));

    REQUIRE(piece.type() == kungfu::PieceType::King);
    REQUIRE(piece.color() == kungfu::PlayerColor::White);
    REQUIRE(piece.position() == kungfu::Position(0, 0));
    REQUIRE(piece.isMovable() == true);

    SECTION("Setting position updates correctly") {
        piece.setPosition(kungfu::Position(1, 1));
        REQUIRE(piece.position() == kungfu::Position(1, 1));
    }
}