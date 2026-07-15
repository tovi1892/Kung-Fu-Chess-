#pragma once

#include <chrono>
#include <cstdint>
#include <unordered_map>

#include "AssetManager.hpp"
#include "model/Enums.hpp"

namespace kungfu {

// Tracks, per piece (by RenderPiece::id), how long it has been in its
// current PieceState using a real wall-clock (independent of the engine's
// simulated time - frames_per_sec is a real display rate, not tied to
// game speed). Resets automatically whenever a piece's state changes.
class PieceAnimator {
public:
    // Returns which frame of `sequence` should be drawn right now for this
    // piece, advancing/resetting its internal clock as a side effect. Call
    // once per piece per render() tick.
    int frameIndexFor(uintptr_t pieceId, PieceState state, const SpriteSequence& sequence);

private:
    struct Entry {
        PieceState state;
        std::chrono::steady_clock::time_point enteredAt;
    };

    std::unordered_map<uintptr_t, Entry> entries_;
};

}  // namespace kungfu
