#pragma once

#include <string>
#include <unordered_map>
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

// Loads and caches every piece's per-state sprite animations from disk.
class AssetManager {
public:
    const PieceAnimationSet& getAnimations(PieceType type, PlayerColor color, int width, int height);

private:
    // Asset folders are named <Kind><Color>, e.g. "PW" for white pawn, "KB" for black king.
    static std::string folderName(PieceType type, PlayerColor color);
    static SpriteSequence loadSequence(const std::string& pieceFolder, const std::string& stateName,
                                        int width, int height);

    std::unordered_map<std::string, PieceAnimationSet> cache_;
};

}  // namespace kungfu
