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

// Client -> server: the only message a client ever sends in Phase 1.
struct ClickMessage {
    int row;
    int col;
};

// Server -> client, sent once right after a connection completes.
struct WelcomeMessage {
    PlayerColor color;
};

// Server -> client, broadcast every server tick - a full render snapshot.
struct StateMessage {
    std::vector<RenderPiece> pieces;
};

using DecodedMessage = std::variant<std::monostate, ClickMessage, WelcomeMessage, StateMessage,
                                     MoveStarted, PieceCaptured, GameStarted, GameEnded>;

std::string encodeClick(int row, int col);
std::string encodeWelcome(PlayerColor color);
std::string encodeState(const std::vector<RenderPiece>& pieces);
std::string encodeMoveStarted(const MoveStarted& event);
std::string encodePieceCaptured(const PieceCaptured& event);
std::string encodeGameStarted();
std::string encodeGameEnded(const GameEnded& event);

// Returns std::monostate (index 0) for anything unrecognized or malformed - callers
// should always check the variant's index before using it.
DecodedMessage decode(const std::string& text);

}  // namespace kungfu::net
