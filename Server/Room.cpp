#include "Room.hpp"

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

}  // namespace kungfu
