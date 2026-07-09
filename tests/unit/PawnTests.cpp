// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include <memory>
#include "board/Board.hpp"
#include "game/Game.hpp"
#include "pieces/King.hpp"
#include "pieces/Pawn.hpp"
#include "pieces/Queen.hpp"
#include "pieces/Rook.hpp"

int main() {
    // --- Double-step from start row is allowed ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(1, 0));
        board->placePiece(pawn, kungfu::Position(1, 0));

        kungfu::Game g(board);
        g.start();
        assert(g.tryMove(kungfu::Position(1, 0), kungfu::Position(3, 0)));
        assert(board->pieceAt(kungfu::Position(3, 0)).has_value());
        assert(!board->pieceAt(kungfu::Position(1, 0)).has_value());
    }

    // --- Double-step from non-start row is NOT allowed ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(3, 0));
        board->placePiece(pawn, kungfu::Position(3, 0));

        kungfu::Game g(board);
        g.start();
        assert(!g.tryMove(kungfu::Position(3, 0), kungfu::Position(5, 0)));
        assert(board->pieceAt(kungfu::Position(3, 0)).has_value());  // pawn didn't move
    }

    // --- Double-step blocked by piece on the intermediate square ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn    = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(1, 0));
        auto blocker = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::Black, kungfu::Position(2, 0));
        board->placePiece(pawn, kungfu::Position(1, 0));
        board->placePiece(blocker, kungfu::Position(2, 0));

        kungfu::Game g(board);
        g.start();
        assert(!g.tryMove(kungfu::Position(1, 0), kungfu::Position(3, 0)));
        assert(board->pieceAt(kungfu::Position(1, 0)).has_value());
    }

    // --- Black pawn double-step is also allowed ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::Black, kungfu::Position(6, 0));
        board->placePiece(pawn, kungfu::Position(6, 0));

        kungfu::Game g(board);
        g.start();
        assert(g.tryMove(kungfu::Position(6, 0), kungfu::Position(4, 0)));
        assert(board->pieceAt(kungfu::Position(4, 0)).has_value());
    }

    // --- Promotion: white pawn reaching last row becomes a queen ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(6, 0));
        board->placePiece(pawn, kungfu::Position(6, 0));

        kungfu::Game g(board);
        g.start();
        assert(g.tryMove(kungfu::Position(6, 0), kungfu::Position(7, 0)));

        const auto promoted = board->pieceAt(kungfu::Position(7, 0));
        assert(promoted.has_value());
        assert(promoted.value()->type() == kungfu::PieceType::Queen);
        assert(promoted.value()->color() == kungfu::PlayerColor::White);
    }

    // --- Promotion: black pawn reaching last row becomes a queen ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::Black, kungfu::Position(1, 0));
        board->placePiece(pawn, kungfu::Position(1, 0));

        kungfu::Game g(board);
        g.start();
        assert(g.tryMove(kungfu::Position(1, 0), kungfu::Position(0, 0)));

        const auto promoted = board->pieceAt(kungfu::Position(0, 0));
        assert(promoted.has_value());
        assert(promoted.value()->type() == kungfu::PieceType::Queen);
        assert(promoted.value()->color() == kungfu::PlayerColor::Black);
    }

    // --- No promotion on non-last row ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(3, 0));
        board->placePiece(pawn, kungfu::Position(3, 0));

        kungfu::Game g(board);
        g.start();
        assert(g.tryMove(kungfu::Position(3, 0), kungfu::Position(4, 0)));

        const auto piece = board->pieceAt(kungfu::Position(4, 0));
        assert(piece.has_value());
        assert(piece.value()->type() == kungfu::PieceType::Pawn);
    }

    // --- Promoted queen can move diagonally ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(6, 1));
        board->placePiece(pawn, kungfu::Position(6, 1));

        kungfu::Game g(board);
        g.start();
        // Promote
        assert(g.tryMove(kungfu::Position(6, 1), kungfu::Position(7, 1)));
        assert(board->pieceAt(kungfu::Position(7, 1)).value()->type() == kungfu::PieceType::Queen);
        // Queen moves diagonally
        assert(g.tryMove(kungfu::Position(7, 1), kungfu::Position(6, 2)));
        assert(board->pieceAt(kungfu::Position(6, 2)).has_value());
        assert(board->pieceAt(kungfu::Position(6, 2)).value()->type() == kungfu::PieceType::Queen);
    }

    // --- Game still continues after promotion (no false game-over) ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto pawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(6, 0));
        auto blackKing = std::make_shared<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(0, 7));
        board->placePiece(pawn, kungfu::Position(6, 0));
        board->placePiece(blackKing, kungfu::Position(0, 7));

        kungfu::Game g(board);
        g.start();
        assert(g.tryMove(kungfu::Position(6, 0), kungfu::Position(7, 0)));
        assert(!g.isFinished());  // game must NOT end just because a pawn promoted
        assert(g.isRunning());
    }

    // --- Pawn cannot capture forward ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whitePawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(1, 0));
        auto enemyRook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::Black, kungfu::Position(2, 0));
        board->placePiece(whitePawn, kungfu::Position(1, 0));
        board->placePiece(enemyRook, kungfu::Position(2, 0));

        kungfu::Game g(board);
        g.start();
        assert(!g.tryMove(kungfu::Position(1, 0), kungfu::Position(2, 0))); // רגלי חסום, אינו יכול לאכול ישר קדימה
        assert(board->pieceAt(kungfu::Position(1, 0)).has_value());
        assert(board->pieceAt(kungfu::Position(2, 0)).has_value());
    }

    // --- Pawn can capture diagonally ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whitePawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(1, 1));
        auto enemyRook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::Black, kungfu::Position(2, 2));
        board->placePiece(whitePawn, kungfu::Position(1, 1));
        board->placePiece(enemyRook, kungfu::Position(2, 2));

        kungfu::Game g(board);
        g.start();
        assert(g.tryMove(kungfu::Position(1, 1), kungfu::Position(2, 2))); // אכילה באלכסון מאושרת
        assert(board->pieceAt(kungfu::Position(2, 2)).has_value());
        assert(board->pieceAt(kungfu::Position(2, 2)).value()->type() == kungfu::PieceType::Pawn);
        assert(!board->pieceAt(kungfu::Position(1, 1)).has_value());
    }

    // --- Pawn cannot move diagonally to an empty square ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whitePawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(1, 1));
        board->placePiece(whitePawn, kungfu::Position(1, 1));

        kungfu::Game g(board);
        g.start();
        assert(!g.tryMove(kungfu::Position(1, 1), kungfu::Position(2, 2))); // אסור לזוז באלכסון למשבצת ריקה
        assert(board->pieceAt(kungfu::Position(1, 1)).has_value());
    }

    // --- Pawn cannot move diagonally to a friendly piece ---
    {
        auto board = std::make_shared<kungfu::Board>();
        auto whitePawn = std::make_shared<kungfu::Pawn>(kungfu::PlayerColor::White, kungfu::Position(1, 1));
        auto friendlyRook = std::make_shared<kungfu::Rook>(kungfu::PlayerColor::White, kungfu::Position(2, 2));
        board->placePiece(whitePawn, kungfu::Position(1, 1));
        board->placePiece(friendlyRook, kungfu::Position(2, 2));

        kungfu::Game g(board);
        g.start();
        assert(!g.tryMove(kungfu::Position(1, 1), kungfu::Position(2, 2))); // אסור לאכול כלי ידידותי באלכסון
        assert(board->pieceAt(kungfu::Position(1, 1)).has_value());
    }

    return 0;
}