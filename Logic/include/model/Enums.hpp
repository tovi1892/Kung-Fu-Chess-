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
    Cooldown,    // arrived from a move; not selectable again until the cooldown elapses
    Airborne,
    ShortRest,   // landed from a jump; not selectable again until the (shorter) rest elapses
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

inline char pieceTypeChar(PieceType type) {
    switch (type) {
        case PieceType::King:   return 'K';
        case PieceType::Queen:  return 'Q';
        case PieceType::Rook:   return 'R';
        case PieceType::Bishop: return 'B';
        case PieceType::Knight: return 'N';
        case PieceType::Pawn:   return 'P';
    }
    return '?';  // unreachable if PieceType stays exhaustive
}

}  // namespace kungfu
