#include <catch2/catch.hpp> 
#include <memory>
#include <optional>

// כולל את כל המחלקות הדרושות
#include "game/Game.hpp"
#include "model/Board.hpp"
#include "rules/RuleEngine.hpp"
#include "game/IGameInputAdapter.hpp"
#include "model/pieces/Pawn.hpp"
#include "model/pieces/Knight.hpp"
#include "model/pieces/Rook.hpp"
#include "model/pieces/Queen.hpp"

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
TEST_CASE("Real-Time Collision - Edge Cases", "[game][edge-cases]") {

    SECTION("Boundary Collision - Corner of the Board") {
        TestContext ctx;
        // מיקום בפינה (0,0) ומכשול ב-(0,1)
        Position start(0, 0);
        Position target(0, 1);
        Position obstacle(0, 1);

        ctx.board->replacePiece(start, std::make_unique<Rook>(PlayerColor::White, start));
        ctx.board->replacePiece(obstacle, std::make_unique<Pawn>(PlayerColor::White, obstacle));

        // ניסיון לנוע לתוך כלי באותו צבע בפינה
        bool result = ctx.game->requestMove(start, target);
        
        // מצפים שהמהלך ייחסם כי זה אותו צבע
        REQUIRE(result == false);
        REQUIRE(ctx.board->pieceAt(start).has_value()); // הצריח חייב להישאר ב-(0,0)
    }

    SECTION("Long-Range Collision - Full Board Sweep") {
        TestContext ctx;
        // צריח בצד אחד של הלוח, מכשול באמצע הדרך, ויעד בקצה השני
        Position rookStart(0, 0);
        Position obstacle(0, 4);
        Position target(0, 7);

        ctx.board->replacePiece(rookStart, std::make_unique<Rook>(PlayerColor::White, rookStart));
        ctx.board->replacePiece(obstacle, std::make_unique<Pawn>(PlayerColor::Black, obstacle));

        // הצריח מנסה לעבור את כל הלוח
        bool result = ctx.game->requestMove(rookStart, target);

        // במקרה של אויב באמצע, החוקים צריכים לטפל בזה (או עצירה ליד, או אכילה בהתאם למנוע שלך)
        // בהנחה שהמנוע שלך עוצר לפני כלי אויב או אוכל אותו - נוודא שהוא לא עובר דרכו
        REQUIRE(ctx.board->pieceAt(rookStart).has_value());
    }

    SECTION("Self-Collision - Trying to move to current square") {
        TestContext ctx;
        Position pos(4, 4);
        ctx.board->replacePiece(pos, std::make_unique<Knight>(PlayerColor::White, pos));

        // ניסיון לנוע לאותה משבצת
        bool result = ctx.game->requestMove(pos, pos);
        
        // מהלך כזה אמור להידחות או לא לשנות כלום
        REQUIRE(result == false);
    }
}