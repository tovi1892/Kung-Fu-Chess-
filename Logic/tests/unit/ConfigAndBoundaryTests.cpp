#include <catch2/catch.hpp>
#include "common/GameConfig.hpp"
#include "movement/MovementSystem.hpp"

TEST_CASE("Boundaries and configuration", "[config]") {
    kungfu::MovementSystem movement;

    REQUIRE(kungfu::GameConfig::kBoardSize == 8);
    REQUIRE(movement.isInBounds(kungfu::Position(0, 0)) == true);
    REQUIRE(movement.isInBounds(kungfu::Position(-1, 0)) == false);
    REQUIRE(movement.isInBounds(kungfu::Position(8, 0)) == false);
}