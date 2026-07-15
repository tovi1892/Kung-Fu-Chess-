#include <catch2/catch.hpp>
#include "model/Position.hpp"

TEST_CASE("Position equality and access", "[position]") {
    kungfu::Position p1(2, 3);
    kungfu::Position p2(2, 3);
    
    REQUIRE(p1 == p2);
    REQUIRE(p1.row() == 2);
    REQUIRE(p1.col() == 3);
}