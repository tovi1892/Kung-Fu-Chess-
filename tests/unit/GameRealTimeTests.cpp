#include <catch2/catch.hpp> 
#include <memory>
#include <optional>

// כולל את כל המחלקות הדרושות
#include "game/Game.hpp"
#include "board/Board.hpp"
#include "rules/RuleEngine.hpp"
#include "game/IGameInputAdapter.hpp"
#include "pieces/Pawn.hpp"
#include "pieces/Knight.hpp"
#include "pieces/Rook.hpp"
#include "pieces/Queen.hpp"

namespace kungfu {
    class DummyInputAdapter : public IGameInputAdapter {
    public:
        void handleClick(int x, int y) override {}
        std::optional<InputCommand> nextCommand() override { return std::nullopt; }
    };
}

using namespace kungfu;

struct TestContext {
    std::shared_ptr<Board> board;
    std::shared_ptr<RuleEngine> ruleEngine;
    std::shared_ptr<DummyInputAdapter> dummyInput;
    std::unique_ptr<Game> game;

    TestContext() {
        board = std::make_shared<Board>();
        ruleEngine = std::make_shared<RuleEngine>(board);
        dummyInput = std::make_shared<DummyInputAdapter>();
        game = std::make_unique<Game>(board, ruleEngine, dummyInput);
        game->start();
    }
};

TEST_CASE("Real-Time Simultaneous Movement and Collision Rules", "[game][realtime]") {
    
    SECTION("Same Color Collision") {
        TestContext ctx;
        Position pawnStart(1, 2);
        Position pawnEnd(2, 2);
        Position knightStart(0, 1);
        Position knightEnd(2, 2);

        // שימוש ב-replacePiece עם שני ארגומנטים כפי שהוגדר
        ctx.board->replacePiece(pawnStart, std::make_unique<Pawn>(PlayerColor::White, pawnStart));
        ctx.board->replacePiece(knightStart, std::make_unique<Knight>(PlayerColor::White, knightStart));

        REQUIRE(ctx.board->pieceAt(pawnStart).has_value());
        REQUIRE(ctx.board->pieceAt(knightStart).has_value());
    }

    SECTION("Different Color Collision") {
        TestContext ctx;
        Position whiteKnightStart(0, 1);
        Position blackPawnStart(6, 3);
        
        ctx.board->replacePiece(whiteKnightStart, std::make_unique<Knight>(PlayerColor::White, whiteKnightStart));
        ctx.board->replacePiece(blackPawnStart, std::make_unique<Pawn>(PlayerColor::Black, blackPawnStart));

        REQUIRE(ctx.board->pieceAt(whiteKnightStart).has_value());
        REQUIRE(ctx.board->pieceAt(blackPawnStart).has_value());
    }
}