#pragma once

#include <string>

#include "model/Enums.hpp"
#include "model/Position.hpp"

namespace kungfu {

// Published by GameEngine::requestMove the instant a move is accepted (same moment it
// records the move into GameRecord). notation is the same string GameRecord stores, so
// a subscriber building a move-log display never needs to recompute it.
struct MoveStarted {
    PlayerColor color;
    PieceType type;
    Position from;
    Position to;
    bool isCapture;
    std::string notation;
    int elapsedMs;
};

// Published by RealTimeArbiter the instant a genuine capture resolves - static, a
// real-time race for a square, a knight landing, or an airborne counter-kill.
struct PieceCaptured {
    PlayerColor capturingColor;
    PieceType capturedType;
    Position at;
};

// Published by GameEngine::start().
struct GameStarted {};

// Published the instant a king capture is detected - see the PieceCaptured subscriber
// GameEngine wires up in its own constructor.
struct GameEnded {
    PlayerColor winner;
};

}  // namespace kungfu
