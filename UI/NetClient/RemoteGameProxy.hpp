#pragma once

#include <mutex>
#include <string>
#include <variant>
#include <vector>

#include "IGameView.hpp"
#include "events/EventBus.hpp"
#include "events/GameEvents.hpp"

#include "Network/WsClientTransport.hpp"

namespace kungfu {

// Both players' usernames, once known - empty strings until the PLAYERS broadcast
// arrives (i.e. until the second player has joined).
struct KnownPlayers {
    std::string white;
    std::string black;
};

// Client-side stand-in for a local GameEngine - deliberately mirrors its public surface
// (onMoveStarted/onPieceCaptured/onGameStarted/onGameEnded/getRenderState) so main.cpp's
// existing subscriber-wiring code barely changes: `game->` becomes `proxy->` and that's
// almost the whole diff.
//
// Thread-safety: WsClientTransport delivers messages on IXWebSocket's own background I/O
// thread. Render-state/color updates are just data, protected by their own small
// mutexes. The four discrete events are different: publishing them immediately from the
// network thread would run every subscriber (SoundPlayer, the score/move-log panels, the
// banner) on that thread too, racing with the main thread's render loop touching the same
// data. Instead they're queued here and only actually published from pollEvents(), which
// main.cpp calls once per frame on the main thread - so every subscriber still only ever
// runs on the one thread it always has.
class RemoteGameProxy {
public:
    // Sends `JOIN <username>` the instant the connection opens - see WsClientTransport's
    // onOpen.
    RemoteGameProxy(const std::string& serverUrl, const std::string& username);

    void sendClick(int row, int col);

    // Call once per frame, from the main thread, before reading render state or
    // scoreboard data that a subscriber might update.
    void pollEvents();

    std::vector<RenderPiece> getRenderState() const;
    KnownPlayers players() const;
    bool hasColor() const;
    PlayerColor myColor() const;

    EventBus<MoveStarted>& onMoveStarted() { return moveBus_; }
    EventBus<PieceCaptured>& onPieceCaptured() { return captureBus_; }
    EventBus<GameStarted>& onGameStarted() { return startBus_; }
    EventBus<GameEnded>& onGameEnded() { return endBus_; }

private:
    void handleMessage(const std::string& text);  // runs on the network thread

    std::string username_;
    net::WsClientTransport transport_;

    EventBus<MoveStarted> moveBus_;
    EventBus<PieceCaptured> captureBus_;
    EventBus<GameStarted> startBus_;
    EventBus<GameEnded> endBus_;

    using QueuedEvent = std::variant<MoveStarted, PieceCaptured, GameStarted, GameEnded>;
    mutable std::mutex queueMutex_;
    std::vector<QueuedEvent> pendingEvents_;

    mutable std::mutex stateMutex_;
    std::vector<RenderPiece> latestState_;

    mutable std::mutex playersMutex_;
    KnownPlayers players_;

    mutable std::mutex colorMutex_;
    PlayerColor myColor_ = PlayerColor::White;
    bool hasColor_ = false;
};

}  // namespace kungfu
