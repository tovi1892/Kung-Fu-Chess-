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
    Idle,
    Moving,
    Cooldown,   // arrived from a move; not selectable again until the cooldown elapses
    Airborne,
    Captured
};

// Check/checkmate are intentionally absent - this game has no such concept;
// the only win condition is capturing the enemy king.
enum class GameState {
    NotStarted,
    Running,
    Paused,
    Finished
};

}  // namespace kungfu
