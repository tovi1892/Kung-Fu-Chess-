#include <mutex>

#include "AccountStore.hpp"
#include "ConnectionSessions.hpp"
#include "GameServer.hpp"
#include "Outbox.hpp"
#include "RoomManager.hpp"

#include "Network/Logger.hpp"
#include "Network/WsServerTransport.hpp"

using namespace kungfu;
using namespace kungfu::net;

namespace {
constexpr int kPort = 7777;
}  // namespace

int main() {
    // Guards every access to roomManager/sessions/outbox below - IXWebSocket delivers
    // connect/message/disconnect callbacks on its own background I/O thread(s), while
    // GameEngine/Controller were built single-threaded. RoomManager/Outbox/
    // ConnectionSessions/GameServer hold no lock of their own - every call into them
    // already runs with this mutex held, exactly as direct access to their former
    // standalone locals did.
    std::mutex gameMutex;

    AccountStore accounts("accounts.db");
    Logger logger("SERVER", "logs/server.log");

    // Tracks each connection's pending -> authenticated identity transition - see
    // ConnectionSessions.hpp for the full boundary.
    ConnectionSessions sessions;

    // Owns every room ever created and the quick-match waiting list - see RoomManager.hpp
    // for the full boundary.
    RoomManager roomManager;

    WsServerTransport server(kPort);

    // Buffers every message queued while gameMutex is held, actually sent only once
    // outbox.flush() is called after it's released - see Outbox.hpp for why.
    Outbox outbox(server, gameMutex);

    // The application layer: translates transport events into domain calls - see
    // GameServer.hpp for the full Ports & Adapters rationale.
    GameServer gameServer(server, roomManager, outbox, sessions, accounts, logger, gameMutex, kPort);
    gameServer.run();
}
