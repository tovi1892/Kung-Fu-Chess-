#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "IGameView.hpp"
#include "events/GameEvents.hpp"
#include "model/Enums.hpp"
#include "model/Position.hpp"

namespace kungfu::net {

// One WebSocket text frame = one message. Reuses Logic's own event structs
// (MoveStarted/PieceCaptured/GameStarted/GameEnded) directly as the decoded payload
// shape - there's no separate "network event" hierarchy to keep in sync with them.

// How a connection wants to be placed into a match, chosen by which button/popup outcome
// the player picked in UsernamePrompt (Play, Room->Create, Room->Join).
enum class JoinMode {
    QuickMatch,
    CreateRoom,
    JoinRoom,
};

// Client -> server: sent once, right after the connection opens. A connection is only
// placed into a room once this arrives (see Server/main.cpp's pendingConnections).
struct JoinMessage {
    std::string username;
    JoinMode mode;
    std::string room;  // meaningful only when mode == JoinRoom
};

// Client -> server: the only other message a client ever sends.
struct ClickMessage {
    int row;
    int col;
};

// Server -> client, sent once right after a connection's JOIN is accepted as a player.
struct WelcomeMessage {
    PlayerColor color;
};

// Server -> client, sent once right after a connection's JOIN is accepted as the 3rd+
// occupant of a room - read-only from here on, no color, no clicks will do anything.
struct SpectateMessage {};

// Server -> client, sent once for any non-quick-match join (Create, Join, or spectating
// a named room) - the room's id, so the client can display it for the whole match.
struct RoomMessage {
    std::string key;
};

// Server -> client, broadcast to both once the second player joins - lets each side's
// score panel show both real usernames instead of the old hardcoded "white"/"black".
struct PlayersMessage {
    std::string white;
    std::string black;
};

// Server -> client, broadcast every server tick - a full render snapshot.
struct StateMessage {
    std::vector<RenderPiece> pieces;
};

using DecodedMessage = std::variant<std::monostate, JoinMessage, ClickMessage, WelcomeMessage,
                                     SpectateMessage, RoomMessage, PlayersMessage, StateMessage,
                                     MoveStarted, PieceCaptured, GameStarted, GameEnded>;

std::string encodeJoin(const std::string& username, JoinMode mode, const std::string& room = "");
std::string encodeClick(int row, int col);
std::string encodeWelcome(PlayerColor color);
std::string encodeSpectate();
std::string encodeRoom(const std::string& key);
std::string encodePlayers(const std::string& white, const std::string& black);
std::string encodeState(const std::vector<RenderPiece>& pieces);
std::string encodeMoveStarted(const MoveStarted& event);
std::string encodePieceCaptured(const PieceCaptured& event);
std::string encodeGameStarted();
std::string encodeGameEnded(const GameEnded& event);

// Returns std::monostate (index 0) for anything unrecognized or malformed - callers
// should always check the variant's index before using it.
DecodedMessage decode(const std::string& text);

}  // namespace kungfu::net
