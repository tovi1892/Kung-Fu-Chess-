#include <catch2/catch.hpp>
#include <optional>
#include "input/Controller.hpp"
#include "model/Board.hpp"
#include "model/pieces/Rook.hpp"

using namespace kungfu;

TEST_CASE("Controller maps a pixel click to a board cell", "[controller]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));

    auto game = std::make_shared<GameEngine>(board, nullptr);
    game->start();
    Controller controller(game);

    // With CELL_SIZE=100, pixel (50,50) maps to cell (0,0).
    REQUIRE(controller.handleClick(50, 50) == true);
    REQUIRE(controller.hasSelection() == true);
    REQUIRE(controller.selectedPosition() == std::optional<Position>(Position(0, 0)));
}

TEST_CASE("Controller owns selection state", "[controller]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));

    auto game = std::make_shared<GameEngine>(board, nullptr);
    game->start();

    Controller controller(game);

    REQUIRE(controller.handleCellClick(0, 0) == true);
    REQUIRE(controller.hasSelection() == true);
    REQUIRE(controller.selectedPosition() == std::optional<Position>(Position(0, 0)));
}

TEST_CASE("Controller cancels selection on an outside-board click", "[controller]") {
    auto board = std::make_shared<Board>();
    board->placePiece(std::make_unique<Rook>(PlayerColor::White, Position(0, 0)), Position(0, 0));

    auto game = std::make_shared<GameEngine>(board, nullptr);
    game->start();
    Controller controller(game);

    REQUIRE(controller.handleCellClick(0, 0) == true);
    REQUIRE(controller.hasSelection() == true);

    REQUIRE(controller.handleCellClick(-1, 0) == false);
    REQUIRE(controller.hasSelection() == false);
}
