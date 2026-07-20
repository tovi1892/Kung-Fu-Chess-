#pragma once

#include <string>
#include <unordered_map>

#include "Core/SpriteSequence.hpp"
#include "model/Enums.hpp"

namespace kungfu {

// Loads and caches every piece's per-state sprite animations from disk.
// This is the OpenCV/Img-backed half of the animation system - the
// SpriteSequence/PieceAnimationSet shapes it populates live in Core/ so a
// future non-OpenCV backend could reuse them with a different loader.
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
