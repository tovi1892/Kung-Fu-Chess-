#pragma once

#include <string>
#include <unordered_map>

#include "SpriteSequence.hpp"
#include "model/Enums.hpp"

namespace kungfu {

// Loads and caches every piece's per-state sprite animations from disk.
// SpriteSequence/PieceAnimationSet (also in this OpenCV/ folder) already hold a
// vector<Img> of decoded frames, so they're just as OpenCV-dependent as this loader -
// there's no backend-agnostic layer being preserved here, just the natural grouping of
// "everything that needs OpenCV" in one place.
class AssetManager {
public:
    // The full set of per-state animations for one piece type+color, sized
    // to width x height. Loaded from disk and cached on first request; every
    // later call for the same type+color returns the cached set.
    const PieceAnimationSet& getAnimations(PieceType type, PlayerColor color, int width, int height);

private:
    // Asset folders are named <Kind><Color>, e.g. "PW" for white pawn, "KB" for black king.
    static std::string folderName(PieceType type, PlayerColor color);
    static SpriteSequence loadSequence(const std::string& pieceFolder, const std::string& stateName,
                                        int width, int height);

    std::unordered_map<std::string, PieceAnimationSet> cache_;
};

}  // namespace kungfu
