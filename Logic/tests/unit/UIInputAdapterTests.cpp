#include <catch2/catch.hpp>
#include <optional>
#include "game/GameController.hpp"
#include "game/IGameInputAdapter.hpp"
#include "game/UIInputAdapter.hpp"
#include "model/Board.hpp"
#include "model/pieces/Rook.hpp"

namespace {
class MockGame : public kungfu::IGameInputTarget {
public:
    bool running = true;
    bool selectCalled = false;
    kungfu::Position lastSelectedPos{};

    bool isRunning() const override { return running; }
    bool isFriendlyPieceAt(const kungfu::Position&) const override { return true; }
    bool selectPiece(const kungfu::Position& pos) override {
        selectCalled = true;
        lastSelectedPos = pos;
        return true;
    }
    bool requestMove(const kungfu::Position&, const kungfu::Position&) override { return false; }
    bool requestJump(const kungfu::Position&) override { return false; }
    bool hasSelection() const override { return false; }
    std::optional<kungfu::Position> selectedPosition() const override { return std::nullopt; }
    bool isPositionInBounds(const kungfu::Position& pos) const override { return true; }
    int boardRows() const override { return 8; }
    int boardCols() const override { return 8; }
};
}

TEST_CASE("UIInputAdapter handles clicks correctly", "[ui]") {
    MockGame game;
    kungfu::UIInputAdapter adapter(game);

    adapter.handleClick(50, 50);

    REQUIRE(game.selectCalled == true);
    REQUIRE(game.lastSelectedPos == kungfu::Position(0, 0));
}

TEST_CASE("GameController owns selection state", "[controller]") {
    auto board = std::make_shared<kungfu::Board>();
    board->placePiece(std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(0, 0)), kungfu::Position(0, 0));

    auto game = std::make_shared<kungfu::Game>(board, nullptr, nullptr);
    game->start();

    kungfu::GameController controller(game);

    REQUIRE(controller.handleCellClick(0, 0) == true);
    REQUIRE(controller.hasSelection() == true);
    REQUIRE(controller.selectedPosition() == std::optional<kungfu::Position>(kungfu::Position(0, 0)));
}