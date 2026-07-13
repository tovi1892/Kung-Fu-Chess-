
#include <cassert>
#include <memory>
#include "board/Board.hpp"
#include "pieces/King.hpp"

int BoardTests_main() {
    kungfu::Board board;
    auto king = std::make_unique<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0));

    assert(board.placePiece(std::move(king), kungfu::Position(0, 0)));
    assert(board.pieceAt(kungfu::Position(0, 0)).has_value());

    assert(!board.placePiece(std::make_unique<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(1, 1)), kungfu::Position(0, 0)));

    assert(board.movePiece(kungfu::Position(0, 0), kungfu::Position(1, 1)));
    assert(!board.pieceAt(kungfu::Position(0, 0)).has_value());
    assert(board.pieceAt(kungfu::Position(1, 1)).has_value());

    assert(board.removePiece(kungfu::Position(1, 1)));
    assert(!board.pieceAt(kungfu::Position(1, 1)).has_value());
    assert(!board.removePiece(kungfu::Position(1, 1)));

    return 0;
}
