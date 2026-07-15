#include "PieceAnimator.hpp"

#include <algorithm>

namespace kungfu {

int PieceAnimator::frameIndexFor(uintptr_t pieceId, PieceState state, const SpriteSequence& sequence) {
    const auto now = std::chrono::steady_clock::now();

    auto it = entries_.find(pieceId);
    if (it == entries_.end() || it->second.state != state) {
        it = entries_.insert_or_assign(pieceId, Entry{state, now}).first;
    }

    if (sequence.frames.empty()) {
        return 0;
    }

    const double elapsedSec = std::chrono::duration<double>(now - it->second.enteredAt).count();
    const int frameCount = static_cast<int>(sequence.frames.size());
    const int rawFrame = static_cast<int>(elapsedSec * sequence.framesPerSec);

    if (sequence.isLoop) {
        return rawFrame % frameCount;
    }
    return std::min(rawFrame, frameCount - 1);
}

}  // namespace kungfu
