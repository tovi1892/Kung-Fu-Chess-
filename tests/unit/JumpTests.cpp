// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include <memory>
#include "board/Board.hpp"
#include "common/Enums.hpp"
#include "game/Game.hpp"
#include "pieces/King.hpp"
#include "pieces/Rook.hpp"

int main() {
    // --- tryJump sets piece to Airborne ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(rook, kungfu::Position(3, 3));

        kungfu::Game g(board);
        g.start();
        assert(g.tryJump(kungfu::Position(3, 3)));
        assert(rook->isAirborne());
        assert(board->pieceAt(kungfu::Position(3, 3)).has_value());  // still on board
    }

    // --- resolveJump resets piece to Idle (no enemy arrived) ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(rook, kungfu::Position(3, 3));

        kungfu::Game g(board);
        g.start();
        g.tryJump(kungfu::Position(3, 3));
        g.resolveJump(kungfu::Position(3, 3));
        assert(!rook->isAirborne());
        assert(rook->state() == kungfu::PieceState::Idle);
        assert(board->pieceAt(kungfu::Position(3, 3)).has_value());
    }

    // --- Airborne piece captures arriving enemy ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whiteRook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        auto blackRook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::Black, kungfu::Position(3, 0));
        board->placePiece(whiteRook, kungfu::Position(3, 3));
        board->placePiece(blackRook, kungfu::Position(3, 0));

        kungfu::Game g(board);
        g.start();
        g.tryJump(kungfu::Position(3, 3));                                         // white rook jumps
        const bool captured = g.handleArrivalAtAirbornCell(                        // black arrives
            kungfu::Position(3, 3), kungfu::Position(3, 0));
        assert(captured);
        assert(!board->pieceAt(kungfu::Position(3, 0)).has_value());               // attacker removed
        assert(board->pieceAt(kungfu::Position(3, 3)).has_value());                // white rook still there
        assert(!whiteRook->isAirborne());                                           // back to Idle
    }

    // --- Airborne piece does NOT capture friendly arriving piece ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whiteRook1 = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        auto whiteRook2 = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 0));
        board->placePiece(whiteRook1, kungfu::Position(3, 3));
        board->placePiece(whiteRook2, kungfu::Position(3, 0));

        kungfu::Game g(board);
        g.start();
        g.tryJump(kungfu::Position(3, 3));
        const bool captured = g.handleArrivalAtAirbornCell(
            kungfu::Position(3, 3), kungfu::Position(3, 0));
        assert(!captured);                                                           // no capture
        assert(board->pieceAt(kungfu::Position(3, 0)).has_value());                 // friendly untouched
    }

    // --- A moving piece cannot jump ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(rook, kungfu::Position(3, 3));

        rook->setState(kungfu::PieceState::Moving);

        kungfu::Game g(board);
        g.start();
        assert(!g.tryJump(kungfu::Position(3, 3)));
        assert(!rook->isAirborne());
    }

    // --- A captured piece cannot jump ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(rook, kungfu::Position(3, 3));

        rook->setState(kungfu::PieceState::Captured);

        kungfu::Game g(board);
        g.start();
        assert(!g.tryJump(kungfu::Position(3, 3)));
    }

    // --- Airborne capture of enemy king ends the game ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whiteRook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto blackKing = std::make_shared<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(0, 7));
        board->placePiece(whiteRook, kungfu::Position(0, 0));
        board->placePiece(blackKing, kungfu::Position(0, 7));

        kungfu::Game g(board);
        g.start();
        g.tryJump(kungfu::Position(0, 0));
        g.handleArrivalAtAirbornCell(kungfu::Position(0, 0), kungfu::Position(0, 7));
        assert(g.isFinished());
    }

    // --- Already-airborne piece cannot jump again ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(rook, kungfu::Position(3, 3));

        kungfu::Game g(board);
        g.start();
        assert(g.tryJump(kungfu::Position(3, 3)));
        assert(!g.tryJump(kungfu::Position(3, 3)));  // second jump rejected
    }

    return 0;
}
