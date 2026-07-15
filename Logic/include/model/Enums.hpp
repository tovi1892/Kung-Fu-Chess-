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

}  // namespace kungfu
