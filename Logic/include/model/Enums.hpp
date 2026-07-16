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

// Check/checkmate are intentionally absent: the course spec's common route
// excludes them entirely (win condition is king capture only).
enum class GameState {
    NotStarted,
    Running,
    Paused,
    Finished
};

}  // namespace kungfu
