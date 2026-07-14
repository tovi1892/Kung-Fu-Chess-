#pragma once

#include <string>
#include <unordered_map>

#include "Img/img.hpp"
#include "common/Enums.hpp"

namespace kungfu {

// Loads and caches the idle sprite for each piece type/color from disk.
class AssetManager {
public:
    Img& getPieceSprite(PieceType type, PlayerColor color, int width, int height) {
        const std::string key = folderName(type, color);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }

        Img sprite;
        sprite.read("UI/assets/pieces1/" + key + "/states/idle/sprites/1.png", {width, height}, true);
        return cache_.emplace(key, std::move(sprite)).first->second;
    }

private:
    // Asset folders are named <Kind><Color>, e.g. "PW" for white pawn, "KB" for black king.
    static std::string folderName(PieceType type, PlayerColor color) {
        char kind = '?';
        switch (type) {
            case PieceType::King:   kind = 'K'; break;
            case PieceType::Queen:  kind = 'Q'; break;
            case PieceType::Rook:   kind = 'R'; break;
            case PieceType::Bishop: kind = 'B'; break;
            case PieceType::Knight: kind = 'N'; break;
            case PieceType::Pawn:   kind = 'P'; break;
        }
        const char colorCh = (color == PlayerColor::White) ? 'W' : 'B';
        return std::string(1, kind) + std::string(1, colorCh);
    }

    std::unordered_map<std::string, Img> cache_;
};

}  // namespace kungfu
