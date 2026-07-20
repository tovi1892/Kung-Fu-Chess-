#pragma once

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

// Client -> server: sent once, right after the connection opens - the username the
// player entered in UsernamePrompt. A connection is only placed into a match once this
// arrives (see Server/main.cpp's pendingConnections).
struct JoinMessage {
    std::string username;
};

// Client -> server: the only other message a client ever sends in Phase 1/2.
struct ClickMessage {
    int row;
    int col;
};

// Server -> client, sent once right after a connection's JOIN is accepted.
struct WelcomeMessage {
    PlayerColor color;
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
                                     PlayersMessage, StateMessage, MoveStarted, PieceCaptured,
                                     GameStarted, GameEnded>;

std::string encodeJoin(const std::string& username);
std::string encodeClick(int row, int col);
std::string encodeWelcome(PlayerColor color);
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
