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

// How a connection wants to be placed into a match, chosen by which option the player
// picked on RoomChoiceScreen (Quick Match, Create Room, Join Room).
enum class JoinMode {
    QuickMatch,
    CreateRoom,
    JoinRoom,
};

// Client -> server: sent once, right after the connection opens - every connection must
// log in before anything else (see Server/main.cpp's pendingLogin state). Auto-registers a
// new account on first use; an existing username must match its stored password.
struct LoginMessage {
    std::string username;
    std::string password;
};

// Server -> client, sent once right after a connection's LOGIN succeeds.
struct LoginOkMessage {
    int rating;
    bool accountCreated;  // true if this LOGIN just auto-registered a brand-new username,
                           // false if it signed into an existing one
};

// Server -> client, sent once right after a connection's LOGIN fails (e.g. wrong password
// for an existing username) - the connection is *not* dropped, it may retry with a
// corrected password on the same socket.
struct LoginFailMessage {
    std::string reason;  // machine-readable, e.g. "bad_password"
};

// Client -> server: sent once a connection is authenticated (see LoginOkMessage). A
// connection is only placed into a room once this arrives (see Server/main.cpp's
// authenticatedConnections). No username field - the server already knows who this
// connection is from its successful LOGIN.
struct JoinMessage {
    JoinMode mode;
    std::string room;  // meaningful only when mode == JoinRoom
};

// Server -> client: a QuickMatch search found nobody within range before its timeout
// elapsed - the client should show a message and let the player retry.
struct NoOpponentMessage {};

// Server -> client, broadcast to a room's remaining occupants the instant a player
// disconnects mid-game - lets the opponent's client render a literal countdown.
struct ForfeitWarningMessage {
    PlayerColor disconnectedColor;
    int graceMs;
};

// Server -> client, broadcast once a disconnect's grace period elapses without the
// disconnected player coming back - winner is whichever color didn't disconnect. A
// separate concept from GameEnded (which only ever means "a king was captured").
struct ForfeitMessage {
    PlayerColor winner;
};

// Server -> client, broadcast to a room the instant a disconnected player reconnects
// (recognized by matching their newly-authenticated username against the room's pending
// forfeit, not by any client-held token - see Server/main.cpp) before the forfeit grace
// period elapsed. Lets the remaining occupants' clients cancel their own
// ForfeitWarning/countdown display immediately, rather than it just sitting there until the
// now-irrelevant deadline stops mattering.
struct ReconnectedMessage {
    PlayerColor color;
};

// Server -> client, broadcast right after GAME_END or FORFEIT - both players' post-match
// Elo ratings, so each client can update its own scoreboard display.
struct RatingsMessage {
    int whiteRating;
    int blackRating;
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

using DecodedMessage = std::variant<std::monostate, LoginMessage, LoginOkMessage, LoginFailMessage,
                                     JoinMessage, ClickMessage, WelcomeMessage, SpectateMessage,
                                     RoomMessage, PlayersMessage, StateMessage, NoOpponentMessage,
                                     ForfeitWarningMessage, ForfeitMessage, ReconnectedMessage, RatingsMessage,
                                     MoveStarted, PieceCaptured, GameStarted, GameEnded>;

std::string encodeLogin(const std::string& username, const std::string& password);
std::string encodeLoginOk(int rating, bool accountCreated);
std::string encodeLoginFail(const std::string& reason);
std::string encodeJoin(JoinMode mode, const std::string& room = "");
std::string encodeClick(int row, int col);
std::string encodeWelcome(PlayerColor color);
std::string encodeSpectate();
std::string encodeRoom(const std::string& key);
std::string encodePlayers(const std::string& white, const std::string& black);
std::string encodeState(const std::vector<RenderPiece>& pieces);
std::string encodeNoOpponent();
std::string encodeForfeitWarning(PlayerColor disconnectedColor, int graceMs);
std::string encodeForfeit(PlayerColor winner);
std::string encodeReconnected(PlayerColor color);
std::string encodeRatings(int whiteRating, int blackRating);
std::string encodeMoveStarted(const MoveStarted& event);
std::string encodePieceCaptured(const PieceCaptured& event);
std::string encodeGameStarted();
std::string encodeGameEnded(const GameEnded& event);

// Returns std::monostate (index 0) for anything unrecognized or malformed - callers
// should always check the variant's index before using it.
DecodedMessage decode(const std::string& text);

}  // namespace kungfu::net
