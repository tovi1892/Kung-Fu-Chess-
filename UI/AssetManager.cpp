#include "AssetManager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace kungfu {

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Minimal, schema-specific field extraction for config.json - not a general
// JSON parser (the project may only depend on OpenCV beyond the standard
// library), just enough to read the two fields under "graphics" that every
// state's config.json is known to have: frames_per_sec and is_loop.
double extractNumberField(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0.0;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return 0.0;
    return std::stod(json.substr(pos + 1));
}

bool extractBoolField(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    auto valuePos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (valuePos == std::string::npos) return false;
    return json.compare(valuePos, 4, "true") == 0;
}

}  // namespace

const SpriteSequence& PieceAnimationSet::forState(PieceState state) const {
    switch (state) {
        case PieceState::Moving:   return move;
        case PieceState::Airborne: return jump;
        case PieceState::Cooldown: return longRest;
        case PieceState::Idle:
        case PieceState::Captured:
        default:
            return idle;
    }
}

std::string AssetManager::folderName(PieceType type, PlayerColor color) {
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

SpriteSequence AssetManager::loadSequence(const std::string& pieceFolder, const std::string& stateName,
                                           int width, int height) {
    SpriteSequence seq;
    const std::string stateDir = "UI/assets/pieces1/" + pieceFolder + "/states/" + stateName;

    const std::string configJson = readFile(stateDir + "/config.json");
    seq.framesPerSec = std::max(1.0, extractNumberField(configJson, "frames_per_sec"));
    seq.isLoop = extractBoolField(configJson, "is_loop");

    for (int i = 1;; ++i) {
        const std::string framePath = stateDir + "/sprites/" + std::to_string(i) + ".png";
        if (!std::filesystem::exists(framePath)) {
            break;
        }
        Img frame;
        frame.read(framePath, {width, height}, true);
        frame.keyOutNearWhite();
        seq.frames.push_back(std::move(frame));
    }

    return seq;
}

const PieceAnimationSet& AssetManager::getAnimations(PieceType type, PlayerColor color, int width, int height) {
    const std::string key = folderName(type, color);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    PieceAnimationSet set;
    set.idle = loadSequence(key, "idle", width, height);
    set.move = loadSequence(key, "move", width, height);
    set.jump = loadSequence(key, "jump", width, height);
    set.longRest = loadSequence(key, "long_rest", width, height);
    set.shortRest = loadSequence(key, "short_rest", width, height);

    return cache_.emplace(key, std::move(set)).first->second;
}

}  // namespace kungfu
