#include <catch2/catch.hpp>
#include "movement/MovementSystem.hpp"
#include "pieces/Bishop.hpp"
#include "pieces/King.hpp"
#include "pieces/Knight.hpp"
#include "pieces/Pawn.hpp"
#include "pieces/Rook.hpp"

TEST_CASE("Movement rules", "[movement]") {
    kungfu::MovementSystem movement;

    SECTION("King movement") {
        kungfu::King king(kungfu::PlayerColor::White, kungfu::Position(0, 0));
        REQUIRE(movement.isValidMove(king, kungfu::Position(0, 0), kungfu::Position(1, 1)));
        REQUIRE(movement.isValidMove(king, kungfu::Position(0, 0), kungfu::Position(2, 2)) == false);
    }

    SECTION("Rook movement") {
        kungfu::Rook rook(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        REQUIRE(movement.isValidMove(rook, kungfu::Position(3, 3), kungfu::Position(3, 7)));
        REQUIRE(movement.isValidMove(rook, kungfu::Position(3, 3), kungfu::Position(4, 7)) == false);
    }

    SECTION("Pawn movement") {
        kungfu::Pawn whitePawn(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        REQUIRE(movement.isValidMove(whitePawn, kungfu::Position(3, 3), kungfu::Position(4, 3)));
        REQUIRE(movement.isValidMove(whitePawn, kungfu::Position(3, 3), kungfu::Position(2, 3)) == false);
        
        kungfu::Pawn whiteStartPawn(kungfu::PlayerColor::White, kungfu::Position(1, 0));
        REQUIRE(movement.isValidMove(whiteStartPawn, kungfu::Position(1, 0), kungfu::Position(3, 0)));
    }
}