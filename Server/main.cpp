#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "AccountStore.hpp"
#include "Room.hpp"

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

// An authenticated (LOGIN_OK'd) connection that hasn't necessarily joined a room yet.
struct AuthenticatedSession {
    std::string username;
    int rating;
};

// A connection searching for a quick-match opponent, waiting to be paired or to time out.
struct WaitingPlayer {
    WsServerTransport::ConnectionId id;
    std::string username;
    int rating;
    std::chrono::steady_clock::time_point deadline;
};

}  // namespace

int main() {
    // Guards every access to rooms/connectionRoom/pendingConnections/authenticatedConnections/
    // quickMatchWaiting/pendingOutbox below - IXWebSocket delivers connect/message/disconnect
    // callbacks on its own background I/O thread(s), while GameEngine/Controller were built
    // single-threaded.
    std::mutex gameMutex;

    AccountStore accounts("accounts.db");
    Logger logger("SERVER", "logs/server.log");

    // A connection lives here from the moment it opens until its LOGIN succeeds (or it
    // disconnects first) - nothing else it sends means anything yet.
    std::unordered_set<WsServerTransport::ConnectionId> pendingConnections;

    // Authenticated connections not yet routed into a room - moves out once JOIN pairs
    // them (quick match) or seats them (Create/Join), but the entry itself lives here for
    // the connection's whole lifetime so onDisconnect/click-routing can always look up
    // "who is this."
    std::unordered_map<WsServerTransport::ConnectionId, AuthenticatedSession> authenticatedConnections;

    // Every room that has ever been created, keyed by room id (quick-match ids are
    // server-generated too, just never shown to anyone - see JoinMode::QuickMatch).
    // Rooms are never removed once created - see the plan's explicit non-goals.
    std::unordered_map<std::string, std::unique_ptr<Room>> rooms;

    // Which room a *joined* connection (player or spectator) belongs to.
    std::unordered_map<WsServerTransport::ConnectionId, std::string> connectionRoom;

    // Quick-match connections currently waiting for an opponent within kEloMatchRangeElo -
    // a linear scan/list, not a sorted structure, since handling many simultaneous matches
    // at scale is explicitly deferred to Phase 5.
    std::vector<WaitingPlayer> quickMatchWaiting;
    int nextQuickMatchId = 0;

    // Next id handed to a Room -> Create - a plain incrementing counter (not a random
    // string) so it's short and easy to read aloud/type to a friend. Never repeats within
    // the process's lifetime, so no uniqueness check needed.
    int nextNamedRoomId = 0;

    // Messages queued while gameMutex is held, actually sent only after it's released (see
    // flushOutbox below). Sending directly while holding gameMutex is what caused a real
    // crash during Phase 4 testing: if ix::WebSocket::send() detects a dead connection, it
    // can synchronously invoke this transport's Close callback on the *same* thread, which
    // tries to lock gameMutex again in onDisconnect - a self-deadlock (the same category of
    // bug already fixed once for WsServerTransport's own clientsMutex_, reintroduced here by
    // calling server.send()/broadcastToRoom() from inside a gameMutex-locked section).
    using Outbox = std::vector<std::pair<WsServerTransport::ConnectionId, std::string>>;
    Outbox pendingOutbox;

    WsServerTransport server(kPort);

    // Assumes gameMutex is already held by the caller - just records intent, sends nothing.
    auto queueSend = [&pendingOutbox](const WsServerTransport::ConnectionId& id, const std::string& text) {
        pendingOutbox.emplace_back(id, text);
    };
    auto queueToRoom = [&pendingOutbox](const Room& room, const std::string& text) {
        for (const auto& [id, session] : room.players) {
            (void)session;
            pendingOutbox.emplace_back(id, text);
        }
        for (const auto& id : room.spectators) {
            pendingOutbox.emplace_back(id, text);
        }
    };
    // Called with gameMutex NOT held - briefly re-locks just long enough to swap out
    // whatever was queued, then sends every message with no lock held at all.
    auto flushOutbox = [&server, &pendingOutbox, &gameMutex]() {
        Outbox outbox;
        {
            std::lock_guard<std::mutex> lock(gameMutex);
            outbox.swap(pendingOutbox);
        }
        for (const auto& [id, text] : outbox) {
            server.send(id, text);
        }
    };

    // Shared by both ways a match can end (a real king capture's GameEnded, or a
    // disconnect countdown elapsing) - looks up both players' usernames from room.players
    // (must still be present - callers run this before any post-match player/spectator
    // bookkeeping), applies the standard Elo update, and queues the resulting ratings.
    // Always called with gameMutex already held.
    auto applyMatchResult = [&accounts, &queueToRoom, &logger](Room& room, PlayerColor winnerColor) {
        std::string winnerName, loserName;
        for (const auto& [id, session] : room.players) {
            (void)id;
            if (session.color == winnerColor) {
                winnerName = session.username;
            } else {
                loserName = session.username;
            }
        }
        if (winnerName.empty() || loserName.empty()) {
            return;  // shouldn't happen - defensive, not a real expected path
        }

        const auto elo = accounts.recordResult(winnerName, loserName);
        const int whiteRating = winnerColor == PlayerColor::White ? elo.newWinnerRating : elo.newLoserRating;
        const int blackRating = winnerColor == PlayerColor::White ? elo.newLoserRating : elo.newWinnerRating;
        queueToRoom(room, encodeRatings(whiteRating, blackRating));
        logger.log("room \"" + room.key + "\": rating update " + winnerName + " -> " +
                   std::to_string(elo.newWinnerRating) + ", " + loserName + " -> " +
                   std::to_string(elo.newLoserRating));
    };

    // Broadcasts every bus event to everyone in that room (players + spectators) the
    // instant it happens - always fires synchronously from within a gameMutex-locked call
    // (room.game->start()/wait()), so queueing here (not sending) is required, not optional.
    // Captures a stable Room* rather than re-looking the key up each time - safe because a
    // Room, once inserted into `rooms` via unique_ptr, never moves, even if the map itself
    // rehashes.
    auto registerRoomBroadcasts = [&queueToRoom, &applyMatchResult, &logger](Room& room) {
        Room* roomPtr = &room;
        room.game->onMoveStarted().subscribe([&queueToRoom, roomPtr](const MoveStarted& event) {
            queueToRoom(*roomPtr, encodeMoveStarted(event));
        });
        room.game->onPieceCaptured().subscribe([&queueToRoom, roomPtr](const PieceCaptured& event) {
            queueToRoom(*roomPtr, encodePieceCaptured(event));
        });
        room.game->onGameStarted().subscribe([&queueToRoom, roomPtr](const GameStarted&) {
            queueToRoom(*roomPtr, encodeGameStarted());
        });
        room.game->onGameEnded().subscribe([&queueToRoom, &applyMatchResult, &logger, roomPtr](const GameEnded& event) {
            queueToRoom(*roomPtr, encodeGameEnded(event));
            logger.log("room \"" + roomPtr->key + "\": game ended, " +
                       (event.winner == PlayerColor::White ? "White" : "Black") + " wins");
            applyMatchResult(*roomPtr, event.winner);
        });
    };

    auto getOrCreateRoom = [&](const std::string& key) -> Room& {
        auto it = rooms.find(key);
        if (it == rooms.end()) {
            auto room = createRoom(key);
            registerRoomBroadcasts(*room);
            it = rooms.emplace(key, std::move(room)).first;
        }
        return *it->second;
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
        pendingConnections.insert(id);
        logger.log("connection " + id + " opened - waiting for LOGIN");
    });

    server.onMessage([&](const WsServerTransport::ConnectionId& id, const std::string& text) {
        // do/while(false) so every early exit below (`break`) still falls through to
        // release the lock and call flushOutbox() - see the Outbox comment above for why
        // sends must never happen while gameMutex is held.
        do {
            std::lock_guard<std::mutex> lock(gameMutex);

            const auto decoded = decode(text);

            // Still waiting to log in: the only message that means anything from this
            // connection is LOGIN - anything else is ignored. A failed login does not
            // disconnect - the same connection may retry with a corrected password.
            if (pendingConnections.count(id) > 0) {
                const auto* login = std::get_if<LoginMessage>(&decoded);
                if (!login) {
                    break;
                }

                const auto result = accounts.login(login->username, login->password);
                if (result.success) {
                    pendingConnections.erase(id);
                    authenticatedConnections[id] = AuthenticatedSession{login->username, result.rating};
                    queueSend(id, encodeLoginOk(result.rating, result.accountCreated));
                    logger.log(login->username + (result.accountCreated ? " registered and logged in (rating " : " logged in (rating ") +
                               std::to_string(result.rating) + ")");
                } else {
                    queueSend(id, encodeLoginFail(result.failureReason));
                    logger.log("login failed for \"" + login->username + "\": " + result.failureReason);
                }
                break;
            }

            // Authenticated but not yet routed into a room: the only message that means
            // anything here is JOIN.
            const auto authIt = authenticatedConnections.find(id);
            if (authIt != authenticatedConnections.end() && connectionRoom.find(id) == connectionRoom.end()) {
                const auto* join = std::get_if<JoinMessage>(&decoded);
                if (!join) {
                    break;
                }
                const std::string username = authIt->second.username;
                const int rating = authIt->second.rating;

                // Reconnection: does this authenticated username match a seat with a
                // pending forfeit, in any room? Recognized by identity (an already-
                // password-authenticated username), not by the client knowing/passing a
                // room id - quick-match rooms never expose one anyway. Deliberately
                // unconditional on join->mode/join->room: whatever the player actually
                // clicked, an active pending-forfeit seat wins, since resuming an existing
                // match within the grace window is what a reconnecting player wants.
                bool reconnected = false;
                for (auto& [roomKey, roomPtr] : rooms) {
                    if (!roomPtr->pendingForfeit.has_value()) {
                        continue;
                    }
                    const auto sessionIt =
                        std::find_if(roomPtr->players.begin(), roomPtr->players.end(), [&](const auto& entry) {
                            return entry.second.username == username &&
                                   entry.second.color == roomPtr->pendingForfeit->disconnectedColor;
                        });
                    if (sessionIt == roomPtr->players.end()) {
                        continue;
                    }

                    PlayerSession session = std::move(sessionIt->second);
                    roomPtr->players.erase(sessionIt);
                    // Clears whatever was selected at the moment of disconnect (attachGame's
                    // documented side effect) - re-attaching the same game it already had,
                    // purely for that side effect.
                    session.controller->attachGame(session.controller->game());
                    const PlayerColor reconnectedColor = session.color;
                    roomPtr->players[id] = std::move(session);
                    connectionRoom[id] = roomKey;
                    roomPtr->pendingForfeit.reset();

                    queueSend(id, encodeWelcome(reconnectedColor));
                    queueSend(id, encodeRoom(roomKey));
                    const auto [whiteName, blackName] = namesOf(*roomPtr);
                    queueSend(id, encodePlayers(whiteName, blackName));
                    queueToRoom(*roomPtr, encodeReconnected(reconnectedColor));
                    logger.log("room \"" + roomKey + "\": " + username +
                               " reconnected - forfeit countdown cancelled");

                    reconnected = true;
                    break;
                }
                if (reconnected) {
                    break;
                }

                if (join->mode == JoinMode::QuickMatch) {
                    const auto waitingIt =
                        std::find_if(quickMatchWaiting.begin(), quickMatchWaiting.end(), [&](const WaitingPlayer& w) {
                            return std::abs(w.rating - rating) <= kEloMatchRangeElo;
                        });

                    if (waitingIt != quickMatchWaiting.end()) {
                        const WaitingPlayer waiting = *waitingIt;
                        quickMatchWaiting.erase(waitingIt);

                        const std::string roomKey = "quickmatch-" + std::to_string(nextQuickMatchId++);
                        Room& room = getOrCreateRoom(roomKey);
                        connectionRoom[waiting.id] = roomKey;
                        connectionRoom[id] = roomKey;
                        room.players[waiting.id] = PlayerSession{waiting.username, PlayerColor::White,
                                                                  std::make_shared<Controller>(room.game)};
                        room.players[id] =
                            PlayerSession{username, PlayerColor::Black, std::make_shared<Controller>(room.game)};

                        queueSend(waiting.id, encodeWelcome(PlayerColor::White));
                        queueSend(id, encodeWelcome(PlayerColor::Black));
                        queueToRoom(room, encodePlayers(waiting.username, username));
                        room.game->start();
                        logger.log("room \"" + roomKey + "\": quick match (" + waiting.username + " vs " + username +
                                   ") - game started");
                    } else {
                        quickMatchWaiting.push_back(WaitingPlayer{
                            id, username, rating, std::chrono::steady_clock::now() + kQuickMatchTimeout});
                        logger.log(username + " searching for a quick-match opponent (rating " +
                                   std::to_string(rating) + ")");
                    }
                    break;
                }

                std::string roomKey;
                if (join->mode == JoinMode::CreateRoom) {
                    roomKey = std::to_string(nextNamedRoomId++);
                } else {
                    roomKey = join->room;
                }

                Room& room = getOrCreateRoom(roomKey);
                connectionRoom[id] = roomKey;

                if (room.players.size() < 2) {
                    const PlayerColor color = room.players.empty() ? PlayerColor::White : PlayerColor::Black;
                    room.players[id] = PlayerSession{username, color, std::make_shared<Controller>(room.game)};
                    logger.log(username + " joined room \"" + roomKey + "\" as " +
                               (color == PlayerColor::White ? "White" : "Black"));

                    queueSend(id, encodeWelcome(color));
                    queueSend(id, encodeRoom(roomKey));

                    if (room.players.size() == 2) {
                        const auto [whiteName, blackName] = namesOf(room);
                        queueToRoom(room, encodePlayers(whiteName, blackName));
                        room.game->start();
                        logger.log("room \"" + roomKey + "\": both players joined (" + whiteName + " vs " +
                                   blackName + ") - game started");
                    }
                } else {
                    room.spectators.insert(id);
                    logger.log(username + " is spectating room \"" + roomKey + "\"");

                    queueSend(id, encodeSpectate());
                    queueSend(id, encodeRoom(roomKey));
                    const auto [whiteName, blackName] = namesOf(room);
                    queueSend(id, encodePlayers(whiteName, blackName));
                }
                break;
            }

            // Already in a room: the only message a joined connection ever sends is a click.
            const auto roomIt = connectionRoom.find(id);
            if (roomIt == connectionRoom.end()) {
                break;  // a rejected/unrecognized connection - ignore whatever it sends
            }
            Room& room = *rooms.at(roomIt->second);

            const auto sessionIt = room.players.find(id);
            if (sessionIt == room.players.end()) {
                break;  // a spectator (or a forfeited match's former players) - clicks do nothing
            }

            const auto* click = std::get_if<ClickMessage>(&decoded);
            if (!click) {
                break;
            }

            PlayerSession& session = sessionIt->second;
            const Position cell(click->row, click->col);

            // The one rule that doesn't exist locally today (one mouse could always move
            // either color): a connection may only ever *select* its own color's pieces.
            // Once a selection is active, it was already validated here, so the second
            // click (the actual move/jump target) can go straight through.
            if (!session.controller->hasSelection()) {
                const auto piece = room.game->getBoard()->pieceAt(cell);
                if (!piece.has_value() || !piece.value() || piece.value()->color() != session.color) {
                    break;
                }
            }

            session.controller->handleCellClick(cell.row(), cell.col());
        } while (false);

        flushOutbox();
    });

    server.onDisconnect([&](const WsServerTransport::ConnectionId& id) {
        {
            std::lock_guard<std::mutex> lock(gameMutex);

            pendingConnections.erase(id);

            quickMatchWaiting.erase(std::remove_if(quickMatchWaiting.begin(), quickMatchWaiting.end(),
                                                    [&](const WaitingPlayer& w) { return w.id == id; }),
                                     quickMatchWaiting.end());

            const auto roomIt = connectionRoom.find(id);
            if (roomIt != connectionRoom.end()) {
                Room& room = *rooms.at(roomIt->second);
                const auto sessionIt = room.players.find(id);

                if (sessionIt != room.players.end() && room.game->isRunning()) {
                    // A player disconnected mid-game: start the forfeit grace period instead
                    // of dropping their seat immediately - their username is still needed
                    // for the eventual Elo update (see decision #4 in the Phase 4 plan).
                    const PlayerColor disconnectedColor = sessionIt->second.color;
                    room.pendingForfeit = PendingForfeit{
                        disconnectedColor, std::chrono::steady_clock::now() + std::chrono::milliseconds(kForfeitGraceMs)};
                    queueToRoom(room, encodeForfeitWarning(disconnectedColor, kForfeitGraceMs));
                    logger.log("room \"" + room.key + "\": " + sessionIt->second.username +
                               " disconnected mid-game - forfeit countdown started");
                } else {
                    room.players.erase(id);
                    room.spectators.erase(id);
                }
                connectionRoom.erase(roomIt);
            }

            // Unconditional, regardless of which branch above ran (or none did): a
            // connection that never joined a room, or whose room already cleared it via a
            // prior forfeit/game-end resolution, must never leave a stale entry behind.
            authenticatedConnections.erase(id);

            logger.log("connection " + id + " disconnected");
        }
        flushOutbox();
    });

    server.start();
    logger.log("listening on port " + std::to_string(kPort));

    while (true) {
        {
            std::lock_guard<std::mutex> lock(gameMutex);
            const auto now = std::chrono::steady_clock::now();

            for (auto it = quickMatchWaiting.begin(); it != quickMatchWaiting.end();) {
                if (now >= it->deadline) {
                    queueSend(it->id, encodeNoOpponent());
                    logger.log(it->username + " quick-match search timed out - no opponent found");
                    it = quickMatchWaiting.erase(it);
                } else {
                    ++it;
                }
            }

            for (auto& [key, roomPtr] : rooms) {
                (void)key;

                if (roomPtr->pendingForfeit.has_value() && now >= roomPtr->pendingForfeit->deadline) {
                    const PlayerColor winner = roomPtr->pendingForfeit->disconnectedColor == PlayerColor::White
                                                    ? PlayerColor::Black
                                                    : PlayerColor::White;
                    queueToRoom(*roomPtr, encodeForfeit(winner));
                    logger.log("room \"" + roomPtr->key + "\": forfeit resolved, " +
                               (winner == PlayerColor::White ? "White" : "Black") + " wins");
                    applyMatchResult(*roomPtr, winner);

                    // Move everyone still tracked into spectators (keeps receiving STATE)
                    // and stop the simulation - GameEngine itself has no idea a forfeit
                    // happened (no king was captured), so without this it would happily
                    // keep running and could later fire a second, conflicting GameEnded.
                    for (const auto& [playerId, session] : roomPtr->players) {
                        (void)session;
                        roomPtr->spectators.insert(playerId);
                    }
                    roomPtr->players.clear();
                    roomPtr->game->stop();
                    roomPtr->pendingForfeit.reset();
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
                    queueSend(id, stateText);
                }
            }
        }
        flushOutbox();
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
    }
}
