#include "GameServer.hpp"

#include <memory>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "ClickRouter.hpp"
#include "MatchResult.hpp"

namespace kungfu {

namespace {

constexpr int kTickMs = 16;

// Matchmaking (decision #3 in the Phase 4 plan).
constexpr int kEloMatchRangeElo = 100;
constexpr auto kQuickMatchTimeout = std::chrono::seconds(60);  // matches the presentation's "1 min"

// Disconnect grace period before an automatic forfeit (decision #4).
constexpr int kForfeitGraceMs = 20000;  // matches the presentation's "20 sec"

// Both player names, White then Black - only meaningful once room.players.size() == 2.
std::pair<std::string, std::string> namesOf(const Room& room) {
    std::string whiteName, blackName;
    for (const auto& [id, session] : room.players) {
        (void)id;
        (session.color == PlayerColor::White ? whiteName : blackName) = session.username;
    }
    return std::make_pair(whiteName, blackName);
}

}  // namespace

GameServer::GameServer(net::WsServerTransport& server, RoomManager& roomManager, Outbox& outbox,
                       ConnectionSessions& sessions, std::shared_ptr<IAccountRepository> accounts,
                       net::Logger& logger, std::mutex& registryMutex, int port)
    : server_(server),
      roomManager_(roomManager),
      outbox_(outbox),
      sessions_(sessions),
      accounts_(std::move(accounts)),
      logger_(logger),
      registryMutex_(registryMutex),
      port_(port) {}

void GameServer::run() {
    server_.onConnect([this](const ConnectionId& id) { handleConnect(id); });
    server_.onMessage([this](const ConnectionId& id, const std::string& text) { handleMessage(id, text); });
    server_.onDisconnect([this](const ConnectionId& id) { handleDisconnect(id); });

    server_.start();
    logger_.log("listening on port " + std::to_string(port_));

    while (true) {
        tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
    }
}

void GameServer::handleConnect(const ConnectionId& id) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    sessions_.markPending(id);
    logger_.log("connection " + id + " opened - waiting for LOGIN");
}

void GameServer::handleMessage(const ConnectionId& id, const std::string& text) {
    const auto decoded = net::decode(text);

    // Room* deliberately outlives the registry lock below: rooms are never destroyed once
    // created (RoomManager's own invariant), so holding this raw pointer after releasing
    // registryMutex_ - right up until roomMutex is locked in its place - is safe.
    Room* clickRoom = nullptr;
    {
        std::lock_guard<std::mutex> lock(registryMutex_);

        // Still waiting to log in: the only message that means anything from this
        // connection is LOGIN - anything else is ignored. A failed login does not
        // disconnect - the same connection may retry with a corrected password.
        if (sessions_.isPending(id)) {
            handleLogin(id, decoded);
        } else if (const auto authSession = sessions_.find(id);
                   authSession.has_value() && !roomManager_.isConnectionRouted(id)) {
            // Authenticated but not yet routed into a room: the only message that means
            // anything here is JOIN.
            handleJoin(id, decoded, authSession->username, authSession->rating);
        } else {
            // Already in a room (or a rejected/unrecognized connection, in which case
            // clickRoom stays null and handleClick is simply never called): the only
            // message a joined connection ever sends is a click.
            clickRoom = roomManager_.roomForConnection(id);
        }
    }
    if (clickRoom != nullptr) {
        std::lock_guard<std::mutex> roomLock(clickRoom->roomMutex);
        handleClick(*clickRoom, id, decoded);
    }
    outbox_.flush();
}

void GameServer::handleDisconnect(const ConnectionId& id) {
    {
        std::lock_guard<std::mutex> lock(registryMutex_);

        roomManager_.removeWaiter(id);

        if (Room* roomPtr = roomManager_.roomForConnection(id); roomPtr != nullptr) {
            Room& room = *roomPtr;
            {
                std::lock_guard<std::mutex> roomLock(room.roomMutex);
                const auto sessionIt = room.players.find(id);

                if (sessionIt != room.players.end() && room.game->isRunning()) {
                    // A player disconnected mid-game: start the forfeit grace period
                    // instead of dropping their seat immediately - their username is still
                    // needed for the eventual Elo update (see decision #4 in the Phase 4
                    // plan).
                    const PlayerColor disconnectedColor = sessionIt->second.color;
                    roomManager_.startForfeitCountdown(room, disconnectedColor, kForfeitGraceMs);
                    outbox_.enqueueToRoom(room, net::encodeForfeitWarning(disconnectedColor, kForfeitGraceMs));
                    logger_.log("room \"" + room.key + "\": " + sessionIt->second.username +
                                " disconnected mid-game - forfeit countdown started");
                } else {
                    room.players.erase(id);
                    room.spectators.erase(id);
                }
            }
            roomManager_.forgetConnectionRoom(id);
        }

        // Unconditional, regardless of which branch above ran (or none did): a
        // connection that never joined a room, or whose room already cleared it via a
        // prior forfeit/game-end resolution, must never leave a stale entry behind -
        // covers both the pending and authenticated cases in one call.
        sessions_.forget(id);

        logger_.log("connection " + id + " disconnected");
    }
    outbox_.flush();
}

void GameServer::handleLogin(const ConnectionId& id, const net::DecodedMessage& decoded) {
    const auto* login = std::get_if<net::LoginMessage>(&decoded);
    if (!login) {
        return;
    }

    const auto result = accounts_->login(login->username, login->password);
    if (result.success) {
        sessions_.authenticate(id, login->username, result.rating);
        outbox_.enqueue(id, net::encodeLoginOk(result.rating, result.accountCreated));
        logger_.log(login->username +
                    (result.accountCreated ? " registered and logged in (rating " : " logged in (rating ") +
                    std::to_string(result.rating) + ")");
    } else {
        outbox_.enqueue(id, net::encodeLoginFail(result.failureReason));
        logger_.log("login failed for \"" + login->username + "\": " + result.failureReason);
    }
}

void GameServer::handleJoin(const ConnectionId& id, const net::DecodedMessage& decoded,
                            const std::string& username, int rating) {
    const auto* join = std::get_if<net::JoinMessage>(&decoded);
    if (!join) {
        return;
    }

    // Reconnection: does this authenticated username match a seat with a pending forfeit,
    // in any room? Recognized by identity (an already-password-authenticated username),
    // not by the client knowing/passing a room id - quick-match rooms never expose one
    // anyway. Deliberately unconditional on join->mode/join->room: whatever the player
    // actually clicked, an active pending-forfeit seat wins, since resuming an existing
    // match within the grace window is what a reconnecting player wants.
    if (tryReconnect(id, username)) {
        return;
    }

    if (join->mode == net::JoinMode::QuickMatch) {
        handleQuickMatch(id, username, rating);
        return;
    }

    handleRoomJoin(id, *join, username);
}

bool GameServer::tryReconnect(const ConnectionId& id, const std::string& username) {
    auto reconnected = roomManager_.reconnect(username, id);
    if (!reconnected.has_value()) {
        return false;
    }

    // reconnected->lock is still held here (see RoomManager::ReconnectResult) - room.players
    // is safe to read for the rest of this function, and released automatically on return.
    Room& room = *reconnected->room;
    const PlayerColor reconnectedColor = reconnected->color;
    roomManager_.assignConnectionToRoom(id, room.key);

    outbox_.enqueue(id, net::encodeWelcome(reconnectedColor));
    outbox_.enqueue(id, net::encodeRoom(room.key));
    const auto [whiteName, blackName] = namesOf(room);
    outbox_.enqueue(id, net::encodePlayers(whiteName, blackName));
    outbox_.enqueueToRoom(room, net::encodeReconnected(reconnectedColor));
    logger_.log("room \"" + room.key + "\": " + username + " reconnected - forfeit countdown cancelled");
    return true;
}

void GameServer::handleQuickMatch(const ConnectionId& id, const std::string& username, int rating) {
    if (const auto waiting = roomManager_.matchWaiter(rating, kEloMatchRangeElo); waiting.has_value()) {
        const std::string roomKey = roomManager_.allocateQuickMatchRoomKey();
        Room& room = roomManager_.getOrCreateRoom(roomKey, [this](Room& r) { registerRoomBroadcasts(r); });
        roomManager_.assignConnectionToRoom(waiting->id, roomKey);
        roomManager_.assignConnectionToRoom(id, roomKey);

        {
            // Even a room this fresh needs its own lock here: GameServer::tick() iterates
            // whatever's currently in RoomManager::rooms() on its own thread regardless of
            // whether this room has finished being set up yet.
            std::lock_guard<std::mutex> roomLock(room.roomMutex);
            room.players[waiting->id] = PlayerSession{waiting->username, PlayerColor::White,
                                                       std::make_shared<Controller>(room.game)};
            room.players[id] = PlayerSession{username, PlayerColor::Black, std::make_shared<Controller>(room.game)};

            outbox_.enqueue(waiting->id, net::encodeWelcome(PlayerColor::White));
            outbox_.enqueue(id, net::encodeWelcome(PlayerColor::Black));
            outbox_.enqueueToRoom(room, net::encodePlayers(waiting->username, username));
            room.game->start();
        }
        logger_.log("room \"" + roomKey + "\": quick match (" + waiting->username + " vs " + username +
                    ") - game started");
    } else {
        roomManager_.addWaiter(
            WaitingPlayer{id, username, rating, std::chrono::steady_clock::now() + kQuickMatchTimeout});
        logger_.log(username + " searching for a quick-match opponent (rating " + std::to_string(rating) + ")");
    }
}

void GameServer::handleRoomJoin(const ConnectionId& id, const net::JoinMessage& join, const std::string& username) {
    const std::string roomKey =
        join.mode == net::JoinMode::CreateRoom ? roomManager_.allocateNamedRoomKey() : join.room;

    Room& room = roomManager_.getOrCreateRoom(roomKey, [this](Room& r) { registerRoomBroadcasts(r); });
    roomManager_.assignConnectionToRoom(id, roomKey);

    // Same "even a fresh room needs its own lock" reasoning as handleQuickMatch above.
    std::lock_guard<std::mutex> roomLock(room.roomMutex);

    if (room.players.size() < 2) {
        const PlayerColor color = room.players.empty() ? PlayerColor::White : PlayerColor::Black;
        room.players[id] = PlayerSession{username, color, std::make_shared<Controller>(room.game)};
        logger_.log(username + " joined room \"" + roomKey + "\" as " +
                    (color == PlayerColor::White ? "White" : "Black"));

        outbox_.enqueue(id, net::encodeWelcome(color));
        outbox_.enqueue(id, net::encodeRoom(roomKey));

        if (room.players.size() == 2) {
            const auto [whiteName, blackName] = namesOf(room);
            outbox_.enqueueToRoom(room, net::encodePlayers(whiteName, blackName));
            room.game->start();
            logger_.log("room \"" + roomKey + "\": both players joined (" + whiteName + " vs " + blackName +
                        ") - game started");
        }
    } else {
        room.spectators.insert(id);
        logger_.log(username + " is spectating room \"" + roomKey + "\"");

        outbox_.enqueue(id, net::encodeSpectate());
        outbox_.enqueue(id, net::encodeRoom(roomKey));
        const auto [whiteName, blackName] = namesOf(room);
        outbox_.enqueue(id, net::encodePlayers(whiteName, blackName));
    }
}

void GameServer::tick() {
    const auto now = std::chrono::steady_clock::now();

    // Snapshotting the room list under registryMutex_ and then releasing it - rather than
    // holding registryMutex_ for the whole sweep - is the actual point of per-room locking:
    // each room is locked (and can be slow, e.g. a forfeit's synchronous Elo write) only for
    // its own advance, never blocking every other room's clicks for the whole tick. Safe to
    // hold these raw Room* past the unlock below, same reasoning as handleMessage's
    // clickRoom - rooms are never destroyed once created.
    std::vector<Room*> roomsSnapshot;
    {
        std::lock_guard<std::mutex> lock(registryMutex_);

        for (const auto& waiting : roomManager_.reapExpiredWaiters(now)) {
            outbox_.enqueue(waiting.id, net::encodeNoOpponent());
            logger_.log(waiting.username + " quick-match search timed out - no opponent found");
        }

        roomsSnapshot.reserve(roomManager_.rooms().size());
        for (auto& [key, roomPtr] : roomManager_.rooms()) {
            (void)key;
            roomsSnapshot.push_back(roomPtr.get());
        }
    }

    for (Room* roomPtr : roomsSnapshot) {
        std::lock_guard<std::mutex> roomLock(roomPtr->roomMutex);
        advanceRoom(*roomPtr, now);
    }
    outbox_.flush();
}

void GameServer::advanceRoom(Room& room, std::chrono::steady_clock::time_point now) {
    if (const auto winner = roomManager_.resolveForfeitIfExpired(room, now); winner.has_value()) {
        outbox_.enqueueToRoom(room, net::encodeForfeit(*winner));
        logger_.log("room \"" + room.key + "\": forfeit resolved, " +
                    (*winner == PlayerColor::White ? "White" : "Black") + " wins");
        applyMatchResult(room, *winner, *accounts_, outbox_, logger_);
        roomManager_.finalizeForfeit(room);
    }

    room.game->wait(kTickMs);  // a no-op until both players have joined and game->start() ran

    std::vector<std::string> recipients;
    recipients.reserve(room.players.size() + room.spectators.size());
    for (const auto& [id, session] : room.players) {
        (void)session;
        recipients.push_back(id);
    }
    for (const auto& id : room.spectators) {
        recipients.push_back(id);
    }
    const std::string stateText = net::encodeState(room.game->getRenderState());
    for (const auto& id : recipients) {
        outbox_.enqueue(id, stateText);
    }
}

void GameServer::registerRoomBroadcasts(Room& room) {
    Room* roomPtr = &room;
    room.game->onMoveStarted().subscribe([this, roomPtr](const MoveStarted& event) {
        outbox_.enqueueToRoom(*roomPtr, net::encodeMoveStarted(event));
    });
    room.game->onPieceCaptured().subscribe([this, roomPtr](const PieceCaptured& event) {
        outbox_.enqueueToRoom(*roomPtr, net::encodePieceCaptured(event));
    });
    room.game->onGameStarted().subscribe([this, roomPtr](const GameStarted&) {
        outbox_.enqueueToRoom(*roomPtr, net::encodeGameStarted());
    });
    room.game->onGameEnded().subscribe([this, roomPtr](const GameEnded& event) {
        outbox_.enqueueToRoom(*roomPtr, net::encodeGameEnded(event));
        logger_.log("room \"" + roomPtr->key + "\": game ended, " +
                    (event.winner == PlayerColor::White ? "White" : "Black") + " wins");
        applyMatchResult(*roomPtr, event.winner, *accounts_, outbox_, logger_);
    });
}

}  // namespace kungfu
