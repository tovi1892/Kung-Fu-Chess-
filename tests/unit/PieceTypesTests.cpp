// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include "pieces/King.hpp"
#include "pieces/Queen.hpp"
#include "pieces/Rook.hpp"
#include "pieces/Bishop.hpp"
#include "pieces/Knight.hpp"
#include "pieces/Pawn.hpp"

int main() {
    kungfu::King king(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    kungfu::Queen queen(kungfu::PlayerColor::Black, kungfu::Position(1, 1));
    kungfu::Rook rook(kungfu::PlayerColor::White, kungfu::Position(2, 2));
    kungfu::Bishop bishop(kungfu::PlayerColor::Black, kungfu::Position(3, 3));
    kungfu::Knight knight(kungfu::PlayerColor::White, kungfu::Position(4, 4));
    kungfu::Pawn pawn(kungfu::PlayerColor::Black, kungfu::Position(5, 5));

    assert(king.type() == kungfu::PieceType::King);
    assert(queen.type() == kungfu::PieceType::Queen);
    assert(rook.type() == kungfu::PieceType::Rook);
    assert(bishop.type() == kungfu::PieceType::Bishop);
    assert(knight.type() == kungfu::PieceType::Knight);
    assert(pawn.type() == kungfu::PieceType::Pawn);

    return 0;
}
