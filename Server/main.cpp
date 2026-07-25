#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "AccountStore.hpp"
#include "ClickRouter.hpp"
#include "ConnectionSessions.hpp"
#include "MatchResult.hpp"
#include "Outbox.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"

#include "Network/Logger.hpp"
#include "Network/Protocol.hpp"
#include "Network/WsServerTransport.hpp"

using namespace kungfu;
using namespace kungfu::net;

namespace {

constexpr int kPort = 7777;
constexpr int kTickMs = 16;

// Matchmaking (decision #3 in the Phase 4 plan).
constexpr int kEloMatchRangeElo = 100;
constexpr auto kQuickMatchTimeout = std::chrono::seconds(60);  // matches the presentation's "1 min"

// Disconnect grace period before an automatic forfeit (decision #4).
constexpr int kForfeitGraceMs = 20000;  // matches the presentation's "20 sec"

}  // namespace

int main() {
    // Guards every access to roomManager/sessions/outbox below - IXWebSocket delivers
    // connect/message/disconnect callbacks on its own background I/O thread(s), while
    // GameEngine/Controller were built single-threaded. RoomManager/Outbox/
    // ConnectionSessions hold no lock of their own - every call into them here already runs
    // with this mutex held, exactly as direct access to their former standalone locals did.
    std::mutex gameMutex;

    AccountStore accounts("accounts.db");
    Logger logger("SERVER", "logs/server.log");

    // Tracks each connection's pending -> authenticated identity transition - see
    // ConnectionSessions.hpp for the full boundary. The authenticated entry lives here for
    // the connection's whole lifetime (until forget() on disconnect), so onDisconnect/
    // click-routing can always look up "who is this."
    ConnectionSessions sessions;

    // Owns every room ever created (rooms are never removed once created - see the plan's
    // explicit non-goals) and the quick-match waiting list (a linear scan/list, not a
    // sorted structure, since handling many simultaneous matches at scale is explicitly
    // deferred to Phase 5) - see RoomManager.hpp for the full boundary.
    RoomManager roomManager;

    WsServerTransport server(kPort);

    // Buffers every message queued while gameMutex is held, actually sent only once
    // outbox.flush() is called after it's released - see Outbox.hpp for why (a real crash
    // during Phase 4 testing: sending directly while holding gameMutex risked a
    // self-deadlock, the same category of bug already fixed once for WsServerTransport's
    // own clientsMutex_).
    Outbox outbox(server, gameMutex);

    // Broadcasts every bus event to everyone in that room (players + spectators) the
    // instant it happens - always fires synchronously from within a gameMutex-locked call
    // (room.game->start()/wait()), so queueing here (not sending) is required, not optional.
    // Captures a stable Room* rather than re-looking the key up each time - safe because a
    // Room, once inserted into RoomManager's map via unique_ptr, never moves, even if the
    // map itself rehashes. Exists specifically to match RoomManager::getOrCreateRoom's
    // onCreated hook shape - stays a lambda here rather than a free function, since it's
    // composition-root wiring (GameEngine's buses -> Outbox/AccountStore/Logger for a
    // freshly created room), not standalone domain logic (see MatchResult.hpp for the part
    // of this that is).
    auto registerRoomBroadcasts = [&outbox, &accounts, &logger](Room& room) {
        Room* roomPtr = &room;
        room.game->onMoveStarted().subscribe([&outbox, roomPtr](const MoveStarted& event) {
            outbox.enqueueToRoom(*roomPtr, encodeMoveStarted(event));
        });
        room.game->onPieceCaptured().subscribe([&outbox, roomPtr](const PieceCaptured& event) {
            outbox.enqueueToRoom(*roomPtr, encodePieceCaptured(event));
        });
        room.game->onGameStarted().subscribe([&outbox, roomPtr](const GameStarted&) {
            outbox.enqueueToRoom(*roomPtr, encodeGameStarted());
        });
        room.game->onGameEnded().subscribe([&outbox, &accounts, &logger, roomPtr](const GameEnded& event) {
            outbox.enqueueToRoom(*roomPtr, encodeGameEnded(event));
            logger.log("room \"" + roomPtr->key + "\": game ended, " +
                       (event.winner == PlayerColor::White ? "White" : "Black") + " wins");
            applyMatchResult(*roomPtr, event.winner, accounts, outbox, logger);
        });
    };

    // Both player names, White then Black - only meaningful once room.players.size() == 2.
    auto namesOf = [](const Room& room) {
        std::string whiteName, blackName;
        for (const auto& [id, session] : room.players) {
            (void)id;
            (session.color == PlayerColor::White ? whiteName : blackName) = session.username;
        }
        return std::make_pair(whiteName, blackName);
    };

    server.onConnect([&](const WsServerTransport::ConnectionId& id) {
        std::lock_guard<std::mutex> lock(gameMutex);
        sessions.markPending(id);
        logger.log("connection " + id + " opened - waiting for LOGIN");
    });

    server.onMessage([&](const WsServerTransport::ConnectionId& id, const std::string& text) {
        // do/while(false) so every early exit below (`break`) still falls through to
        // release the lock and call outbox.flush() - see Outbox.hpp for why sends must
        // never happen while gameMutex is held.
        do {
            std::lock_guard<std::mutex> lock(gameMutex);

            const auto decoded = decode(text);

            // Still waiting to log in: the only message that means anything from this
            // connection is LOGIN - anything else is ignored. A failed login does not
            // disconnect - the same connection may retry with a corrected password.
            if (sessions.isPending(id)) {
                const auto* login = std::get_if<LoginMessage>(&decoded);
                if (!login) {
                    break;
                }

                const auto result = accounts.login(login->username, login->password);
                if (result.success) {
                    sessions.authenticate(id, login->username, result.rating);
                    outbox.enqueue(id, encodeLoginOk(result.rating, result.accountCreated));
                    logger.log(login->username + (result.accountCreated ? " registered and logged in (rating " : " logged in (rating ") +
                               std::to_string(result.rating) + ")");
                } else {
                    outbox.enqueue(id, encodeLoginFail(result.failureReason));
                    logger.log("login failed for \"" + login->username + "\": " + result.failureReason);
                }
                break;
            }

            // Authenticated but not yet routed into a room: the only message that means
            // anything here is JOIN.
            const auto authSession = sessions.find(id);
            if (authSession.has_value() && !roomManager.isConnectionRouted(id)) {
                const auto* join = std::get_if<JoinMessage>(&decoded);
                if (!join) {
                    break;
                }
                const std::string username = authSession->username;
                const int rating = authSession->rating;

                // Reconnection: does this authenticated username match a seat with a
                // pending forfeit, in any room? Recognized by identity (an already-
                // password-authenticated username), not by the client knowing/passing a
                // room id - quick-match rooms never expose one anyway. Deliberately
                // unconditional on join->mode/join->room: whatever the player actually
                // clicked, an active pending-forfeit seat wins, since resuming an existing
                // match within the grace window is what a reconnecting player wants.
                if (const auto reconnected = roomManager.reconnect(username, id); reconnected.has_value()) {
                    Room& room = *reconnected->room;
                    const PlayerColor reconnectedColor = reconnected->color;
                    roomManager.assignConnectionToRoom(id, room.key);

                    outbox.enqueue(id, encodeWelcome(reconnectedColor));
                    outbox.enqueue(id, encodeRoom(room.key));
                    const auto [whiteName, blackName] = namesOf(room);
                    outbox.enqueue(id, encodePlayers(whiteName, blackName));
                    outbox.enqueueToRoom(room, encodeReconnected(reconnectedColor));
                    logger.log("room \"" + room.key + "\": " + username +
                               " reconnected - forfeit countdown cancelled");
                    break;
                }

                if (join->mode == JoinMode::QuickMatch) {
                    if (const auto waiting = roomManager.matchWaiter(rating, kEloMatchRangeElo); waiting.has_value()) {
                        const std::string roomKey = roomManager.allocateQuickMatchRoomKey();
                        Room& room = roomManager.getOrCreateRoom(roomKey, registerRoomBroadcasts);
                        roomManager.assignConnectionToRoom(waiting->id, roomKey);
                        roomManager.assignConnectionToRoom(id, roomKey);
                        room.players[waiting->id] = PlayerSession{waiting->username, PlayerColor::White,
                                                                   std::make_shared<Controller>(room.game)};
                        room.players[id] =
                            PlayerSession{username, PlayerColor::Black, std::make_shared<Controller>(room.game)};

                        outbox.enqueue(waiting->id, encodeWelcome(PlayerColor::White));
                        outbox.enqueue(id, encodeWelcome(PlayerColor::Black));
                        outbox.enqueueToRoom(room, encodePlayers(waiting->username, username));
                        room.game->start();
                        logger.log("room \"" + roomKey + "\": quick match (" + waiting->username + " vs " + username +
                                   ") - game started");
                    } else {
                        roomManager.addWaiter(WaitingPlayer{
                            id, username, rating, std::chrono::steady_clock::now() + kQuickMatchTimeout});
                        logger.log(username + " searching for a quick-match opponent (rating " +
                                   std::to_string(rating) + ")");
                    }
                    break;
                }

                std::string roomKey;
                if (join->mode == JoinMode::CreateRoom) {
                    roomKey = roomManager.allocateNamedRoomKey();
                } else {
                    roomKey = join->room;
                }

                Room& room = roomManager.getOrCreateRoom(roomKey, registerRoomBroadcasts);
                roomManager.assignConnectionToRoom(id, roomKey);

                if (room.players.size() < 2) {
                    const PlayerColor color = room.players.empty() ? PlayerColor::White : PlayerColor::Black;
                    room.players[id] = PlayerSession{username, color, std::make_shared<Controller>(room.game)};
                    logger.log(username + " joined room \"" + roomKey + "\" as " +
                               (color == PlayerColor::White ? "White" : "Black"));

                    outbox.enqueue(id, encodeWelcome(color));
                    outbox.enqueue(id, encodeRoom(roomKey));

                    if (room.players.size() == 2) {
                        const auto [whiteName, blackName] = namesOf(room);
                        outbox.enqueueToRoom(room, encodePlayers(whiteName, blackName));
                        room.game->start();
                        logger.log("room \"" + roomKey + "\": both players joined (" + whiteName + " vs " +
                                   blackName + ") - game started");
                    }
                } else {
                    room.spectators.insert(id);
                    logger.log(username + " is spectating room \"" + roomKey + "\"");

                    outbox.enqueue(id, encodeSpectate());
                    outbox.enqueue(id, encodeRoom(roomKey));
                    const auto [whiteName, blackName] = namesOf(room);
                    outbox.enqueue(id, encodePlayers(whiteName, blackName));
                }
                break;
            }

            // Already in a room: the only message a joined connection ever sends is a click.
            handleClick(id, decoded, roomManager);
        } while (false);

        outbox.flush();
    });

    server.onDisconnect([&](const WsServerTransport::ConnectionId& id) {
        {
            std::lock_guard<std::mutex> lock(gameMutex);

            roomManager.removeWaiter(id);

            if (Room* roomPtr = roomManager.roomForConnection(id); roomPtr != nullptr) {
                Room& room = *roomPtr;
                const auto sessionIt = room.players.find(id);

                if (sessionIt != room.players.end() && room.game->isRunning()) {
                    // A player disconnected mid-game: start the forfeit grace period instead
                    // of dropping their seat immediately - their username is still needed
                    // for the eventual Elo update (see decision #4 in the Phase 4 plan).
                    const PlayerColor disconnectedColor = sessionIt->second.color;
                    roomManager.startForfeitCountdown(room, disconnectedColor, kForfeitGraceMs);
                    outbox.enqueueToRoom(room, encodeForfeitWarning(disconnectedColor, kForfeitGraceMs));
                    logger.log("room \"" + room.key + "\": " + sessionIt->second.username +
                               " disconnected mid-game - forfeit countdown started");
                } else {
                    room.players.erase(id);
                    room.spectators.erase(id);
                }
                roomManager.forgetConnectionRoom(id);
            }

            // Unconditional, regardless of which branch above ran (or none did): a
            // connection that never joined a room, or whose room already cleared it via a
            // prior forfeit/game-end resolution, must never leave a stale entry behind -
            // covers both the pending and authenticated cases in one call.
            sessions.forget(id);

            logger.log("connection " + id + " disconnected");
        }
        outbox.flush();
    });

    server.start();
    logger.log("listening on port " + std::to_string(kPort));

    while (true) {
        {
            std::lock_guard<std::mutex> lock(gameMutex);
            const auto now = std::chrono::steady_clock::now();

            for (const auto& waiting : roomManager.reapExpiredWaiters(now)) {
                outbox.enqueue(waiting.id, encodeNoOpponent());
                logger.log(waiting.username + " quick-match search timed out - no opponent found");
            }

            for (auto& [key, roomPtr] : roomManager.rooms()) {
                (void)key;

                if (const auto winner = roomManager.resolveForfeitIfExpired(*roomPtr, now); winner.has_value()) {
                    outbox.enqueueToRoom(*roomPtr, encodeForfeit(*winner));
                    logger.log("room \"" + roomPtr->key + "\": forfeit resolved, " +
                               (*winner == PlayerColor::White ? "White" : "Black") + " wins");
                    applyMatchResult(*roomPtr, *winner, accounts, outbox, logger);
                }

                roomPtr->game->wait(kTickMs);  // a no-op until both players have joined and game->start() ran

                std::vector<std::string> recipients;
                recipients.reserve(roomPtr->players.size() + roomPtr->spectators.size());
                for (const auto& [id, session] : roomPtr->players) {
                    (void)session;
                    recipients.push_back(id);
                }
                for (const auto& id : roomPtr->spectators) {
                    recipients.push_back(id);
                }
                const std::string stateText = encodeState(roomPtr->game->getRenderState());
                for (const auto& id : recipients) {
                    outbox.enqueue(id, stateText);
                }
            }
        }
        outbox.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
    }
}
