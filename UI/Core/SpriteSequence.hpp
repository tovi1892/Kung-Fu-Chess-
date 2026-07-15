#pragma once

#include <vector>

#include "Img/img.hpp"
#include "model/Enums.hpp"

namespace kungfu {

// One playable animation: an ordered list of frames plus how fast/whether to
// loop through them, taken straight from the asset's states/<name>/config.json.
struct SpriteSequence {
    std::vector<Img> frames;
    double framesPerSec = 1.0;
    bool isLoop = true;
};

// Every animation state available for one piece type+color.
struct PieceAnimationSet {
    SpriteSequence idle;
    SpriteSequence move;
    SpriteSequence jump;
    SpriteSequence longRest;
    // Present in the assets but not currently reachable: the engine's
    // Airborne("jump")/PieceState never transitions into a cooldown, so
    // nothing maps to short_rest today.
    SpriteSequence shortRest;

    const SpriteSequence& forState(PieceState state) const;
};

}  // namespace kungfu
