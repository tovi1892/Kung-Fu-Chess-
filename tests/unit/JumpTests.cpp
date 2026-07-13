// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include <memory>
#include "board/Board.hpp"
#include "common/Enums.hpp"
#include "game/Game.hpp"
#include "pieces/King.hpp"
#include "pieces/Rook.hpp"
#include "TestHelpers.hpp"

int JumpTests_main() {
    // --- tryJump sets piece to Airborne ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(std::move(rook), kungfu::Position(3, 3));

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        assert(g->tryJump(kungfu::Position(3, 3)));
        auto* rookPtr = board->pieceAt(kungfu::Position(3, 3)).value();
        assert(rookPtr->isAirborne());
        assert(board->pieceAt(kungfu::Position(3, 3)).has_value());  // still on board
    }

    // --- resolveJump resets piece to Idle (no enemy arrived) ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(std::move(rook), kungfu::Position(3, 3));

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        g->tryJump(kungfu::Position(3, 3));
        g->resolveJump(kungfu::Position(3, 3));
        auto* rookPtr = board->pieceAt(kungfu::Position(3, 3)).value();
        assert(!rookPtr->isAirborne());
        assert(rookPtr->state() == kungfu::PieceState::Idle);
        assert(board->pieceAt(kungfu::Position(3, 3)).has_value());
    }

    // --- Airborne piece captures arriving enemy ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whiteRook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        auto blackRook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::Black, kungfu::Position(3, 0));
        board->placePiece(std::move(whiteRook), kungfu::Position(3, 3));
        board->placePiece(std::move(blackRook), kungfu::Position(3, 0));

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        g->tryJump(kungfu::Position(3, 3));                                         // white rook jumps
        const bool captured = g->handleArrivalAtAirbornCell(                        // black arrives
            kungfu::Position(3, 3), kungfu::Position(3, 0));
        assert(captured);
        assert(!board->pieceAt(kungfu::Position(3, 0)).has_value());               // attacker removed
        assert(board->pieceAt(kungfu::Position(3, 3)).has_value());                // white rook still there
        assert(!whiteRook->isAirborne());                                           // back to Idle
    }

    // --- Airborne piece does NOT capture friendly arriving piece ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whiteRook1 = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        auto whiteRook2 = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 0));
        board->placePiece(std::move(whiteRook1), kungfu::Position(3, 3));
        board->placePiece(std::move(whiteRook2), kungfu::Position(3, 0));

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        g->tryJump(kungfu::Position(3, 3));
        const bool captured = g->handleArrivalAtAirbornCell(
            kungfu::Position(3, 3), kungfu::Position(3, 0));
        assert(!captured);                                                           // no capture
        assert(board->pieceAt(kungfu::Position(3, 0)).has_value());                 // friendly untouched
    }

    // --- A moving piece cannot jump ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(std::move(rook), kungfu::Position(3, 3));

        auto* rookPtr = board->pieceAt(kungfu::Position(3, 3)).value();
        rookPtr->setState(kungfu::PieceState::Moving);

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        assert(!g->tryJump(kungfu::Position(3, 3)));
        assert(!rook->isAirborne());
    }

    // --- A captured piece cannot jump ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(std::move(rook), kungfu::Position(3, 3));

        auto* rookPtr = board->pieceAt(kungfu::Position(3, 3)).value();
        rookPtr->setState(kungfu::PieceState::Captured);

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        assert(!g->tryJump(kungfu::Position(3, 3)));
    }

    // --- Airborne capture of enemy king ends the game ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whiteRook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto blackKing = std::make_unique<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(0, 7));
        board->placePiece(std::move(whiteRook), kungfu::Position(0, 0));
        board->placePiece(std::move(blackKing), kungfu::Position(0, 7));

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        g->tryJump(kungfu::Position(0, 0));
        g->handleArrivalAtAirbornCell(kungfu::Position(0, 0), kungfu::Position(0, 7));
        assert(g->isFinished());
    }

    // --- Already-airborne piece cannot jump again ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto rook = std::make_unique<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(3, 3));
        board->placePiece(std::move(rook), kungfu::Position(3, 3));

        auto g = kungfu::createGameWithAdapter(board);
        g->start();
        assert(g->tryJump(kungfu::Position(3, 3)));
        assert(!g->tryJump(kungfu::Position(3, 3)));  // second jump rejected
    }

    return 0;
}
