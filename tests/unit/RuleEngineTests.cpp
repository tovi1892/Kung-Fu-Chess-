#include <catch2/catch.hpp>
#include "board/Board.hpp"
#include "pieces/King.hpp"
#include "rules/RuleEngine.hpp"

TEST_CASE("Rule engine validation", "[rules]") {
    auto board = std::make_shared<kungfu::Board>();
    board->placePiece(std::make_unique<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0)), kungfu::Position(0, 0));
    kungfu::RuleEngine engine(board);

    SECTION("Valid move returns ok") {
        auto result = engine.validateMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
        REQUIRE(result.is_valid == true);
        REQUIRE(result.reason == "ok");
    }

    SECTION("Illegal move returns error") {
        auto result = engine.validateMove(kungfu::Position(0, 0), kungfu::Position(2, 2));
        REQUIRE(result.is_valid == false);
        REQUIRE(result.reason == "illegal_piece_move");
    }
}