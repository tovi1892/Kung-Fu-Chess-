#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include "ConnectionSessions.hpp"
#include "IAccountRepository.hpp"
#include "Outbox.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"

#include "Network/Logger.hpp"
#include "Network/Protocol.hpp"
#include "Network/WsServerTransport.hpp"

namespace kungfu {

// The application layer in this project's Ports & Adapters shape: WsServerTransport is the
// adapter (knows only connection ids and raw text, nothing about rooms/accounts/game
// logic); RoomManager/Outbox/ConnectionSessions/MatchResult/ClickRouter are the domain
// (know nothing about WebSockets). GameServer is the one class allowed to know about both
// sides - its job is translating transport events (a connection opening, a message
// arriving, a connection dropping, a tick elapsing) into domain calls, and nothing else.
//
// Not thread-safe on its own: every handler locks gameMutex (given at construction) for
// its own body, then releases it before flushing outbox - same contract every domain class
// in Server/ already documents. GameServer introduces no new locking of its own; it's
// exactly the dispatch logic that used to live in Server/main.cpp's lambdas, now named and
// given a home.
class GameServer {
public:
    // port is used only for the startup log line - WsServerTransport itself is already
    // bound to it by the time `server` is passed in here.
    GameServer(net::WsServerTransport& server, RoomManager& roomManager, Outbox& outbox,
               ConnectionSessions& sessions, std::shared_ptr<IAccountRepository> accounts, net::Logger& logger,
               std::mutex& gameMutex, int port);

    // Registers onConnect/onMessage/onDisconnect on the transport, starts it listening,
    // then blocks forever running the tick loop - the last call main() makes.
    [[noreturn]] void run();

private:
    using ConnectionId = net::WsServerTransport::ConnectionId;

    // --- transport callbacks - one per WsServerTransport event -----------------------
    void handleConnect(const ConnectionId& id);
    void handleMessage(const ConnectionId& id, const std::string& text);
    void handleDisconnect(const ConnectionId& id);

    // --- handleMessage's three mutually-exclusive branches ---------------------------
    // (pending -> LOGIN only; authenticated-but-unrouted -> JOIN only; otherwise -> a
    // click, routed through ClickRouter::handleClick, a no-op if not actually routed)
    void handleLogin(const ConnectionId& id, const net::DecodedMessage& decoded);
    void handleJoin(const ConnectionId& id, const net::DecodedMessage& decoded,
                    const std::string& username, int rating);

    // --- handleJoin's three mutually-exclusive outcomes ------------------------------
    bool tryReconnect(const ConnectionId& id, const std::string& username);  // true = handled
    void handleQuickMatch(const ConnectionId& id, const std::string& username, int rating);
    void handleRoomJoin(const ConnectionId& id, const net::JoinMessage& join, const std::string& username);

    // --- tick loop --------------------------------------------------------------------
    void tick();
    void advanceRoom(Room& room, std::chrono::steady_clock::time_point now);

    // --- room-creation wiring hook (RoomManager::getOrCreateRoom's onCreated) --------
    void registerRoomBroadcasts(Room& room);

    net::WsServerTransport& server_;
    RoomManager& roomManager_;
    Outbox& outbox_;
    ConnectionSessions& sessions_;
    std::shared_ptr<IAccountRepository> accounts_;
    net::Logger& logger_;
    std::mutex& gameMutex_;
    int port_;
};

}  // namespace kungfu
