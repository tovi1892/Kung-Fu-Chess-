// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include "movement/MovementSystem.hpp"
#include "pieces/Bishop.hpp"
#include "pieces/King.hpp"
#include "pieces/Knight.hpp"
#include "pieces/Pawn.hpp"
#include "pieces/Rook.hpp"

int main() {
    kungfu::MovementSystem movement;

    kungfu::King king(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    assert(movement.isValidMove(king, kungfu::Position(0, 0), kungfu::Position(1, 1)));
    assert(!movement.isValidMove(king, kungfu::Position(0, 0), kungfu::Position(2, 2)));

    kungfu::Rook rook(kungfu::PlayerColor::White, kungfu::Position(3, 3));
    assert(movement.isValidMove(rook, kungfu::Position(3, 3), kungfu::Position(3, 7)));
    assert(!movement.isValidMove(rook, kungfu::Position(3, 3), kungfu::Position(4, 7)));

    kungfu::Bishop bishop(kungfu::PlayerColor::Black, kungfu::Position(2, 2));
    assert(movement.isValidMove(bishop, kungfu::Position(2, 2), kungfu::Position(5, 5)));
    assert(!movement.isValidMove(bishop, kungfu::Position(2, 2), kungfu::Position(5, 4)));

    kungfu::Knight knight(kungfu::PlayerColor::White, kungfu::Position(4, 4));
    assert(movement.isValidMove(knight, kungfu::Position(4, 4), kungfu::Position(2, 3)));
    assert(!movement.isValidMove(knight, kungfu::Position(4, 4), kungfu::Position(3, 3)));

    // --- Pawn single step ---
    kungfu::Pawn whitePawn(kungfu::PlayerColor::White, kungfu::Position(3, 3));
    assert(movement.isValidMove(whitePawn, kungfu::Position(3, 3), kungfu::Position(4, 3)));
    assert(!movement.isValidMove(whitePawn, kungfu::Position(3, 3), kungfu::Position(2, 3)));  // wrong direction
    assert(!movement.isValidMove(whitePawn, kungfu::Position(3, 3), kungfu::Position(4, 4)));  // diagonal

    kungfu::Pawn blackPawn(kungfu::PlayerColor::Black, kungfu::Position(4, 4));
    assert(movement.isValidMove(blackPawn, kungfu::Position(4, 4), kungfu::Position(3, 4)));
    assert(!movement.isValidMove(blackPawn, kungfu::Position(4, 4), kungfu::Position(5, 4)));  // wrong direction

    // --- Pawn double step from start row ---
    kungfu::Pawn whiteStartPawn(kungfu::PlayerColor::White, kungfu::Position(1, 0));
    assert(movement.isValidMove(whiteStartPawn, kungfu::Position(1, 0), kungfu::Position(3, 0)));   // double step allowed
    assert(!movement.isValidMove(whiteStartPawn, kungfu::Position(1, 0), kungfu::Position(4, 0)));  // triple step not allowed

    kungfu::Pawn blackStartPawn(kungfu::PlayerColor::Black, kungfu::Position(6, 0));
    assert(movement.isValidMove(blackStartPawn, kungfu::Position(6, 0), kungfu::Position(4, 0)));   // double step allowed
    assert(!movement.isValidMove(blackStartPawn, kungfu::Position(6, 0), kungfu::Position(3, 0)));  // triple step not allowed

    // --- Pawn double step NOT allowed from non-start row ---
    kungfu::Pawn whiteMiddlePawn(kungfu::PlayerColor::White, kungfu::Position(3, 0));
    assert(!movement.isValidMove(whiteMiddlePawn, kungfu::Position(3, 0), kungfu::Position(5, 0)));
    assert(!movement.isValidMove(whiteMiddlePawn, kungfu::Position(3, 0), kungfu::Position(6, 0)));  // triple step also not allowed

    return 0;
}
