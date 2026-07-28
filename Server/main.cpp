#include <memory>
#include <mutex>

#include "ConnectionSessions.hpp"
#include "EnvConfig.hpp"
#include "GameServer.hpp"
#include "Outbox.hpp"
#include "RoomManager.hpp"

#ifdef KUNGFU_ENABLE_POSTGRES
#include "PostgresAccountRepository.hpp"
#else
#include "SqliteAccountRepository.hpp"
#endif

#include "Network/Logger.hpp"
#include "Network/WsServerTransport.hpp"

using namespace kungfu;
using namespace kungfu::net;

int main() {
    // Guards cross-room state only - RoomManager's rooms_ map/connectionRoom_/
    // quickMatchWaiting_, and ConnectionSessions. Each Room additionally carries its own
    // roomMutex (see Room.hpp) protecting that room's players/spectators/pendingForfeit and
    // game engine, so one room's click processing never blocks another's. RoomManager/
    // ConnectionSessions/GameServer hold no lock of their own - every call into them already
    // runs with the appropriate lock(s) held, per GameServer.hpp's locking contract.
    std::mutex registryMutex;

    // DB_HOST/DB_PORT/DB_USER/DB_PASSWORD/DB_NAME/PORT from the environment, falling back to
    // the same defaults a plain local run has always used - see EnvConfig.hpp.
    const EnvConfig config = loadEnvConfig();

    // Concrete backing store injected here as the sole IAccountRepository implementation -
    // swapping it for a different backend means writing a new class and changing only this
    // block, per IAccountRepository.hpp's rationale. Which backend even gets compiled in is
    // decided at build time by KUNGFU_ENABLE_POSTGRES (see CMakeLists.txt) - the native
    // Windows dev build never links or includes the Postgres path at all.
#ifdef KUNGFU_ENABLE_POSTGRES
    auto accounts = std::make_shared<PostgresAccountRepository>(config.dbHost, config.dbPort, config.dbUser,
                                                                  config.dbPassword, config.dbName);
#else
    auto accounts = std::make_shared<SqliteAccountRepository>("accounts.db");
#endif
    Logger logger("SERVER", "logs/server.log");

    // Tracks each connection's pending -> authenticated identity transition - see
    // ConnectionSessions.hpp for the full boundary.
    ConnectionSessions sessions;

    // Owns every room ever created and the quick-match waiting list - see RoomManager.hpp
    // for the full boundary.
    RoomManager roomManager;

    WsServerTransport server(config.port);

    // Buffers every message queued while some other lock is held, actually sent only once
    // outbox.flush() is called after it's released - see Outbox.hpp for why it now owns a
    // private mutex of its own rather than sharing registryMutex.
    Outbox outbox(server);

    // The application layer: translates transport events into domain calls - see
    // GameServer.hpp for the full Ports & Adapters rationale.
    GameServer gameServer(server, roomManager, outbox, sessions, accounts, logger, registryMutex, config.port);
    gameServer.run();
}
