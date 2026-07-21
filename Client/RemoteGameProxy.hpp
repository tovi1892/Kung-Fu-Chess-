#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "IGameView.hpp"
#include "events/EventBus.hpp"
#include "events/GameEvents.hpp"

#include "Network/Protocol.hpp"
#include "Network/WsClientTransport.hpp"

namespace kungfu {

// Both players' usernames, once known - empty strings until the PLAYERS broadcast
// arrives (i.e. until the second player has joined).
struct KnownPlayers {
    std::string white;
    std::string black;
};

// Both players' post-match ratings, once known - 0/0 (the "not yet known" sentinel
// PlayerPanel::rating also uses) until a RATINGS message arrives, i.e. until a match this
// client is part of (as a player or spectator) actually concludes.
struct KnownRatings {
    int white = 0;
    int black = 0;
};

// A player disconnected mid-game and the room's forfeit countdown is running - purely
// polled state (like roomKey_ below), not a bus event, since it needs live "how long is
// left" display rather than a one-shot notification. deadline is this client's own local
// clock estimate (steady_clock::now() + graceMs at the moment FORFEIT_WARNING arrived) -
// the actual forfeit decision remains server-authoritative regardless of any local drift.
struct ForfeitWarning {
    PlayerColor disconnectedColor;
    std::chrono::steady_clock::time_point deadline;
};

// Client-side stand-in for a local GameEngine - deliberately mirrors its public surface
// (onMoveStarted/onPieceCaptured/onGameStarted/onGameEnded/getRenderState) so main.cpp's
// existing subscriber-wiring code barely changes: `game->` becomes `proxy->` and that's
// almost the whole diff.
//
// Thread-safety: WsClientTransport delivers messages on IXWebSocket's own background I/O
// thread. Render-state/color updates are just data, protected by their own small
// mutexes. The discrete bus events are different: publishing them immediately from the
// network thread would run every subscriber (SoundPlayer, the score/move-log panels, the
// banner) on that thread too, racing with the main thread's render loop touching the same
// data. Instead they're queued here and only actually published from pollEvents(), which
// main.cpp calls once per frame on the main thread - so every subscriber still only ever
// runs on the one thread it always has.
class RemoteGameProxy {
public:
    // Sends `LOGIN <username> <password>` the instant the connection opens - see
    // WsClientTransport's onOpen. `JOIN <mode> [room]` is sent only after LOGIN_OK
    // arrives; mode/room are stored and replayed at that point.
    RemoteGameProxy(const std::string& serverUrl, const std::string& username, const std::string& password,
                     net::JoinMode mode, const std::string& room);

    void sendClick(int row, int col);

    // Call once per frame, from the main thread, before reading render state or
    // scoreboard data that a subscriber might update.
    void pollEvents();

    std::vector<RenderPiece> getRenderState() const;
    KnownPlayers players() const;

    // True once LOGIN_OK or LOGIN_FAIL has been received.
    bool loginResolved() const;
    bool loginFailed() const;
    std::string loginFailureReason() const;
    int myRating() const;       // this account's own rating - known as soon as LOGIN_OK arrives
    KnownRatings ratings() const;  // both sides' post-match ratings - known once RATINGS arrives

    // True once the server has told this connection its role - either a color (a
    // player) or that it's spectating. main.cpp's connect-wait loop blocks on this
    // rather than hasColor() alone, since a spectator never receives a color.
    bool hasRole() const;
    bool isSpectator() const;
    PlayerColor myColor() const;  // only meaningful when !isSpectator()

    // True once a NO_OPPONENT arrives - a quick-match search timed out with nobody found.
    bool searchFailed() const;

    // The current match's room id, once known - std::nullopt for a quick match (which
    // has no user-facing id) or until the ROOM message arrives for a named room.
    std::optional<std::string> roomKey() const;

    // Set while a room's forfeit countdown is running - std::nullopt otherwise (including
    // once the forfeit actually resolves; see handleMessage's ForfeitMessage branch).
    std::optional<ForfeitWarning> forfeitWarning() const;

    EventBus<MoveStarted>& onMoveStarted() { return moveBus_; }
    EventBus<PieceCaptured>& onPieceCaptured() { return captureBus_; }
    EventBus<GameStarted>& onGameStarted() { return startBus_; }
    EventBus<GameEnded>& onGameEnded() { return endBus_; }
    // A forfeit-resolved win - handled like onGameEnded() for banner/sound purposes, just
    // with different text (see main.cpp) since it wasn't a real king capture.
    EventBus<net::ForfeitMessage>& onForfeit() { return forfeitBus_; }

private:
    void handleMessage(const std::string& text);  // runs on the network thread

    std::string username_;
    net::WsClientTransport transport_;

    // What to JOIN with once LOGIN_OK arrives - set once in the constructor's onOpen
    // callback, read once in handleMessage's LoginOkMessage branch, both on the network
    // thread only (never touched from the main thread), so no mutex is needed for these.
    net::JoinMode pendingJoinMode_ = net::JoinMode::QuickMatch;
    std::string pendingJoinRoom_;

    EventBus<MoveStarted> moveBus_;
    EventBus<PieceCaptured> captureBus_;
    EventBus<GameStarted> startBus_;
    EventBus<GameEnded> endBus_;
    EventBus<net::ForfeitMessage> forfeitBus_;

    using QueuedEvent = std::variant<MoveStarted, PieceCaptured, GameStarted, GameEnded, net::ForfeitMessage>;
    mutable std::mutex queueMutex_;
    std::vector<QueuedEvent> pendingEvents_;

    mutable std::mutex stateMutex_;
    std::vector<RenderPiece> latestState_;

    mutable std::mutex playersMutex_;
    KnownPlayers players_;

    // Groups every "who/where am I" fact this connection learns from the server, all set
    // only from handleMessage (network thread) and read only from the main thread.
    mutable std::mutex sessionMutex_;
    bool loginResolved_ = false;
    bool loginFailed_ = false;
    std::string loginFailureReason_;
    int rating_ = 0;
    KnownRatings ratings_;
    PlayerColor myColor_ = PlayerColor::White;
    bool hasColor_ = false;
    bool isSpectator_ = false;
    bool searchFailed_ = false;
    std::optional<std::string> roomKey_;
    std::optional<ForfeitWarning> forfeitWarning_;
};

}  // namespace kungfu
