#pragma once

namespace kungfu {

enum class PieceType {
    King,
    Queen,
    Rook,
    Bishop,
    Knight,
    Pawn
};

enum class PlayerColor {
    White,
    Black
};

enum class PieceState {
    Idle,       // On the board, not moving.
    Moving,     // En route to a destination.
    Airborne,   // Jumping: stays in place, captures any enemy that arrives.
    Captured    // Removed from the game.
};

}  // namespace kungfu
