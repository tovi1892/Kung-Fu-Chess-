#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <sstream>

#include "texttests/ScriptRunner.hpp"

// Text-based integration tests (course spec sections 13-16). Each script
// drives the real user-facing path - Board parsing, click, wait, print board -
// through ScriptRunner, and "print board" output is the only assertion
// mechanism, exactly as the spec requires.

namespace {

std::string runScript(const char* script) {
    std::istringstream in(script);
    std::ostringstream out;
    REQUIRE(kungfu::ScriptRunner().run(in, out));
    return out.str();
}

}  // namespace

TEST_CASE("01: board parsing", "[integration][parsing]") {
    const char* script = R"(Board:
wK . .
. wR .
. . bK

Commands:
print board
)";

    const std::string expected =
        "wK . .\n"
        ". wR .\n"
        ". . bK\n";

    REQUIRE(runScript(script) == expected);
}

TEST_CASE("02: click-to-move", "[integration][click]") {
    const char* script = R"(Board:
. wR .
. . .
. . bK

Commands:
click 150 50
click 150 150
wait 1000
print board
)";

    const std::string expected =
        ". . .\n"
        ". wR .\n"
        ". . bK\n";

    REQUIRE(runScript(script) == expected);
}

TEST_CASE("03: rook slides multiple squares", "[integration][rook]") {
    const char* script = R"(Board:
wR . . .
. . . .
. . . .
. . . bK

Commands:
click 50 50
click 350 50
wait 3000
print board
)";

    const std::string expected =
        ". . . wR\n"
        ". . . .\n"
        ". . . .\n"
        ". . . bK\n";

    REQUIRE(runScript(script) == expected);
}

TEST_CASE("04: invalid move leaves the board unchanged", "[integration][invalid]") {
    const char* script = R"(Board:
wR wP . .
. . . .
. . . .
. . . bK

Commands:
click 50 50
click 350 50
wait 3000
print board
)";

    // The friendly pawn at (0,1) blocks the rook's path to (0,3), so the
    // move is illegal and the board must print exactly as it started.
    const std::string expected =
        "wR wP . .\n"
        ". . . .\n"
        ". . . .\n"
        ". . . bK\n";

    REQUIRE(runScript(script) == expected);
}

TEST_CASE("05: capture removes the enemy piece", "[integration][capture]") {
    const char* script = R"(Board:
wR . . .
. . . .
. . . .
bP . . bK

Commands:
click 50 50
click 50 350
wait 3000
print board
)";

    const std::string expected =
        ". . . .\n"
        ". . . .\n"
        ". . . .\n"
        "wR . . bK\n";

    REQUIRE(runScript(script) == expected);
}

TEST_CASE("06: king capture ends the game", "[integration][game_over]") {
    const char* script = R"(Board:
wR . . .
. . . .
. . . .
bK . . wK

Commands:
click 50 50
click 50 350
wait 3000
print board
click 350 350
click 250 350
wait 1000
print board
)";

    const std::string afterKingCapture =
        ". . . .\n"
        ". . . .\n"
        ". . . .\n"
        "wR . . wK\n";

    // A second, otherwise-legal-looking move attempted after game-over must
    // be rejected outright, so the board is identical both times.
    const std::string expected = afterKingCapture + afterKingCapture;

    REQUIRE(runScript(script) == expected);
}
