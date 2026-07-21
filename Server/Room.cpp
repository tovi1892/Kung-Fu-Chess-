#include "Room.hpp"

#include <random>
#include <sstream>

#include "io/BoardParser.hpp"

namespace kungfu {

namespace {

// Standard chess starting position - every room starts here.
const char* kInitialBoard = R"(Board:
wR wN wB wQ wK wB wN wR
wP wP wP wP wP wP wP wP
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
bP bP bP bP bP bP bP bP
bR bN bB bQ bK bB bN bR
)";

constexpr int kRoomKeyLength = 6;
constexpr char kRoomKeyAlphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
constexpr int kRoomKeyAlphabetSize = sizeof(kRoomKeyAlphabet) - 1;  // excludes the trailing '\0'

}  // namespace

std::unique_ptr<Room> createRoom(const std::string& key) {
    auto room = std::make_unique<Room>();
    room->key = key;

    std::istringstream boardStream(kInitialBoard);
    BoardParser().parseInput(boardStream, room->board);  // kInitialBoard is a fixed, known-valid layout

    room->ruleEngine = std::make_shared<RuleEngine>(room->board);
    room->game = std::make_shared<GameEngine>(room->board, room->ruleEngine);
    return room;
}

std::string generateRoomKey(const std::unordered_map<std::string, std::unique_ptr<Room>>& existing) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> pick(0, kRoomKeyAlphabetSize - 1);

    std::string key;
    do {
        key.clear();
        key.reserve(kRoomKeyLength);
        for (int i = 0; i < kRoomKeyLength; ++i) {
            key += kRoomKeyAlphabet[pick(rng)];
        }
    } while (existing.count(key) > 0);
    return key;
}

}  // namespace kungfu
