// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include <memory>
#include "board/Board.hpp"
#include "game/Game.hpp"
#include "pieces/King.hpp"
#include "pieces/Rook.hpp"

int main() {
    // --- Basic state machine ---
    kungfu::Game game;
    assert(!game.isRunning());
    assert(!game.isFinished());
    assert(!game.tryMove(kungfu::Position(0, 0), kungfu::Position(1, 1)));

    game.start();
    assert(game.isRunning());

    game.stop();
    assert(!game.isRunning());
    assert(!game.tryMove(kungfu::Position(0, 0), kungfu::Position(1, 1)));

    const auto beforeStart = kungfu::Game{};
    assert(!beforeStart.isRunning());

    // --- Capturing enemy king ends the game ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whiteKing = std::make_shared<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto blackKing = std::make_shared<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(1, 1));
        board->placePiece(whiteKing, kungfu::Position(0, 0));
        board->placePiece(blackKing, kungfu::Position(1, 1));

        kungfu::Game g(board);
        g.start();
        assert(g.isRunning());

        const bool captured = g.tryMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
        assert(captured);
        assert(g.isFinished());
        assert(!g.isRunning());
    }

    // --- After game over, further moves are ignored ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whiteKing = std::make_shared<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto blackKing = std::make_shared<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(1, 1));
        auto whiteRook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(7, 7));
        board->placePiece(whiteKing, kungfu::Position(0, 0));
        board->placePiece(blackKing, kungfu::Position(1, 1));
        board->placePiece(whiteRook, kungfu::Position(7, 7));

        kungfu::Game g(board);
        g.start();
        g.tryMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
        assert(g.isFinished());
        assert(!g.tryMove(kungfu::Position(7, 7), kungfu::Position(7, 0)));
    }

    // --- Rook path clearance block ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto blocker = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::Black, kungfu::Position(0, 3));
        board->placePiece(rook, kungfu::Position(0, 0));
        board->placePiece(blocker, kungfu::Position(0, 3));

        kungfu::Game g(board);
        g.start();
        assert(!g.tryMove(kungfu::Position(0, 0), kungfu::Position(0, 5))); // נפסל כיוון שהנתיב חסום ב-(0,3)
        assert(g.tryMove(kungfu::Position(0, 0), kungfu::Position(0, 2)));  // מאושר כיוון שהנתיב פנוי
    }

    // --- Interactive click and wait delay ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
        board->placePiece(rook, kungfu::Position(0, 0));

        kungfu::Game g(board);
        g.start();

        // המרה: תא (0,0) מיוצג ע"י קואורדינטת פיקסל 50,50. יעד (0,1) מיוצג ע"י פיקסל 150,50.
        g.click(50, 50);  // סימון הצריח ב-(0,0)
        g.click(150, 50); // מתן פקודת תנועה ל-(0,1) - השהייה של 1000 מילישניות (מרחק תא 1)

        // בדיקה 1: לפני שהזמן עובר, הכלי עדיין ב-(0,0) ומצבו "Moving"
        assert(board->pieceAt(kungfu::Position(0, 0)).has_value());
        assert(board->pieceAt(kungfu::Position(0, 0)).value()->state() == kungfu::PieceState::Moving);
        assert(!board->pieceAt(kungfu::Position(0, 1)).has_value());

        // בדיקה 2: המתנה של 500 מילישניות בלבד - הכלי עדיין לא הגיע ליעד
        g.wait(500);
        assert(board->pieceAt(kungfu::Position(0, 0)).has_value());
        assert(!board->pieceAt(kungfu::Position(0, 1)).has_value());

        // בדיקה 3: המתנה של 500 מילישניות נוספות (סך הכל 1000) - הכלי הגיע ליעד ומצבו חזר ל-"Idle"
        g.wait(500);
        assert(!board->pieceAt(kungfu::Position(0, 0)).has_value());
        assert(board->pieceAt(kungfu::Position(0, 1)).has_value());
        assert(board->pieceAt(kungfu::Position(0, 1)).value()->state() == kungfu::PieceState::Idle);
    }

    return 0;
}