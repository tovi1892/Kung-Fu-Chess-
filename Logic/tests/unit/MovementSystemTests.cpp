#include <catch2/catch.hpp>
#include "movement/MovementSystem.hpp"

TEST_CASE("MovementSystem bounds and geometry helpers", "[movement]") {
    kungfu::MovementSystem movement;

    SECTION("isInBounds respects the default 8x8 board") {
        REQUIRE(movement.isInBounds(kungfu::Position(0, 0)));
        REQUIRE(movement.isInBounds(kungfu::Position(7, 7)));
        REQUIRE(movement.isInBounds(kungfu::Position(8, 0)) == false);
        REQUIRE(movement.isInBounds(kungfu::Position(0, -1)) == false);
    }

    SECTION("isInBounds respects a custom board size") {
        REQUIRE(movement.isInBounds(kungfu::Position(2, 2), 3, 3));
        REQUIRE(movement.isInBounds(kungfu::Position(3, 0), 3, 3) == false);
    }

    SECTION("canMoveTo rejects moving to the same cell") {
        REQUIRE(movement.canMoveTo(kungfu::Position(1, 1), kungfu::Position(1, 1)) == false);
        REQUIRE(movement.canMoveTo(kungfu::Position(1, 1), kungfu::Position(1, 2)));
    }

    SECTION("pawnDoubleStepMiddle finds the skipped cell") {
        auto middle = movement.pawnDoubleStepMiddle(kungfu::Position(1, 0), kungfu::Position(3, 0));
        REQUIRE(middle.has_value());
        REQUIRE(*middle == kungfu::Position(2, 0));

        REQUIRE(movement.pawnDoubleStepMiddle(kungfu::Position(1, 0), kungfu::Position(2, 0)).has_value() == false);
    }
}
