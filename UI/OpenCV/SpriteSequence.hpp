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
    // Played while a piece is in PieceState::ShortRest - the brief rest a
    // piece takes right after landing from a jump, shorter than the
    // post-move longRest above.
    SpriteSequence shortRest;

    const SpriteSequence& forState(PieceState state) const;
};

}  // namespace kungfu
