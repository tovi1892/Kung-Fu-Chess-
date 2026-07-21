#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "engine/GameEngine.hpp"
#include "input/Controller.hpp"
#include "model/Board.hpp"
#include "model/Enums.hpp"
#include "rules/RuleEngine.hpp"

namespace kungfu {

// One connected, *joined* player of a Room: their username, which color they were
// assigned, and their own Controller. Both players' Controllers wrap the same shared
// GameEngine - Controller was already designed to own nothing but its own selection
// state, so this needs zero Logic/ changes.
struct PlayerSession {
    std::string username;
    PlayerColor color;
    std::shared_ptr<Controller> controller;
};

// One match: its own board/rules/engine, up to two players, and any number of read-only
// spectators. Connection ids are plain strings (matching
// WsServerTransport::ConnectionId) rather than including Network/ here - this file stays
// pure Logic-composition, with zero networking awareness.
struct Room {
    std::string key;
    std::shared_ptr<Board> board;
    std::shared_ptr<RuleEngine> ruleEngine;
    std::shared_ptr<GameEngine> game;
    std::unordered_map<std::string, PlayerSession> players;  // connection id -> session, max 2
    std::unordered_set<std::string> spectators;               // connection id, read-only
};

// Builds a fresh Room with a new starting board/RuleEngine/GameEngine - no players yet.
std::unique_ptr<Room> createRoom(const std::string& key);

// A short random id not already present in `existing` (retries on the astronomically
// rare collision) - used only for Create, where the client supplied no id of its own.
std::string generateRoomKey(const std::unordered_map<std::string, std::unique_ptr<Room>>& existing);

}  // namespace kungfu
