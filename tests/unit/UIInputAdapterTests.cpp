#include <catch2/catch.hpp>
#include <optional>
#include "game/IGameInputAdapter.hpp"
#include "game/UIInputAdapter.hpp"

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
};
}

TEST_CASE("UIInputAdapter handles clicks correctly", "[ui]") {
    MockGame game;
    kungfu::UIInputAdapter adapter(game);

    adapter.handleClick(50, 50);

    REQUIRE(game.selectCalled == true);
    REQUIRE(game.lastSelectedPos == kungfu::Position(0, 0));
}