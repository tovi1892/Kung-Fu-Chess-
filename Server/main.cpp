#include <chrono>
#include <iostream>
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

#include "Room.hpp"

#include "Network/Protocol.hpp"
#include "Network/WsServerTransport.hpp"

using namespace kungfu;
using namespace kungfu::net;

namespace {

constexpr int kPort = 7777;
constexpr int kTickMs = 16;

}  // namespace

int main() {
    // Guards every access to rooms/connectionRoom/pendingConnections/openQuickMatchKey
    // below - IXWebSocket delivers connect/message/disconnect callbacks on its own
    // background I/O thread(s), while GameEngine/Controller were built single-threaded.
    std::mutex gameMutex;

    // A connection lives here from the moment it opens until its JOIN arrives (or it
    // disconnects first) - it isn't in any room yet, and its clicks are ignored, until
    // it has actually joined.
    std::unordered_set<WsServerTransport::ConnectionId> pendingConnections;

    // Every room that has ever been created, keyed by room id (quick-match ids are
    // server-generated too, just never shown to anyone - see JoinMode::QuickMatch).
    // Rooms are never removed once created - see the plan's explicit non-goals.
    std::unordered_map<std::string, std::unique_ptr<Room>> rooms;

    // Which room a *joined* connection (player or spectator) belongs to.
    std::unordered_map<WsServerTransport::ConnectionId, std::string> connectionRoom;

    // A quick-match room currently waiting for its 2nd player, if any. Deliberately the
    // simplest possible matchmaking: one waiting slot, not a queue/pool - matches "not
    // yet: Elo-based matchmaking".
    std::optional<std::string> openQuickMatchKey;
    int nextQuickMatchId = 0;

    WsServerTransport server(kPort);

    auto broadcastToRoom = [&server](const Room& room, const std::string& text) {
        for (const auto& [id, session] : room.players) {
            (void)session;
            server.send(id, text);
        }
        for (const auto& id : room.spectators) {
            server.send(id, text);
        }
    };

    // Broadcasts every bus event to everyone in that room (players + spectators) the
    // instant it happens. Captures a stable Room* rather than re-looking the key up each
    // time - safe because a Room, once inserted into `rooms` via unique_ptr, never moves,
    // even if the map itself rehashes.
    auto registerRoomBroadcasts = [&broadcastToRoom](Room& room) {
        Room* roomPtr = &room;
        room.game->onMoveStarted().subscribe([&broadcastToRoom, roomPtr](const MoveStarted& event) {
            broadcastToRoom(*roomPtr, encodeMoveStarted(event));
        });
        room.game->onPieceCaptured().subscribe([&broadcastToRoom, roomPtr](const PieceCaptured& event) {
            broadcastToRoom(*roomPtr, encodePieceCaptured(event));
        });
        room.game->onGameStarted().subscribe([&broadcastToRoom, roomPtr](const GameStarted&) {
            broadcastToRoom(*roomPtr, encodeGameStarted());
        });
        room.game->onGameEnded().subscribe([&broadcastToRoom, roomPtr](const GameEnded& event) {
            broadcastToRoom(*roomPtr, encodeGameEnded(event));
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
        std::cout << "[server] connection " << id << " opened - waiting for JOIN" << std::endl;
    });

    server.onMessage([&](const WsServerTransport::ConnectionId& id, const std::string& text) {
        std::lock_guard<std::mutex> lock(gameMutex);

        const auto decoded = decode(text);

        // Still waiting to join: the only message that means anything from this
        // connection is JOIN - anything else (e.g. a stray click before the username
        // window closed) is ignored.
        if (pendingConnections.count(id) > 0) {
            const auto* join = std::get_if<JoinMessage>(&decoded);
            if (!join) {
                return;
            }
            pendingConnections.erase(id);

            std::string roomKey;
            switch (join->mode) {
                case JoinMode::QuickMatch:
                    if (openQuickMatchKey.has_value()) {
                        roomKey = *openQuickMatchKey;
                        openQuickMatchKey.reset();  // about to have 2 players
                    } else {
                        roomKey = "quickmatch-" + std::to_string(nextQuickMatchId++);
                        openQuickMatchKey = roomKey;
                    }
                    break;
                case JoinMode::CreateRoom:
                    roomKey = generateRoomKey(rooms);
                    break;
                case JoinMode::JoinRoom:
                    roomKey = join->room;
                    break;
            }

            Room& room = getOrCreateRoom(roomKey);
            connectionRoom[id] = roomKey;
            const bool isNamedRoom = join->mode != JoinMode::QuickMatch;

            if (room.players.size() < 2) {
                const PlayerColor color = room.players.empty() ? PlayerColor::White : PlayerColor::Black;
                room.players[id] = PlayerSession{join->username, color, std::make_shared<Controller>(room.game)};
                std::cout << "[server] " << join->username << " joined room \"" << roomKey << "\" as "
                          << (color == PlayerColor::White ? "White" : "Black") << std::endl;

                server.send(id, encodeWelcome(color));
                if (isNamedRoom) {
                    server.send(id, encodeRoom(roomKey));
                }

                if (room.players.size() == 2) {
                    const auto [whiteName, blackName] = namesOf(room);
                    broadcastToRoom(room, encodePlayers(whiteName, blackName));
                    room.game->start();
                    std::cout << "[server] room \"" << roomKey << "\": both players joined (" << whiteName
                              << " vs " << blackName << ") - game started" << std::endl;
                }
            } else {
                room.spectators.insert(id);
                std::cout << "[server] " << join->username << " is spectating room \"" << roomKey << "\""
                          << std::endl;

                server.send(id, encodeSpectate());
                if (isNamedRoom) {
                    server.send(id, encodeRoom(roomKey));
                }
                const auto [whiteName, blackName] = namesOf(room);
                server.send(id, encodePlayers(whiteName, blackName));
            }
            return;
        }

        // Already joined: the only message a joined connection ever sends is a click.
        const auto roomIt = connectionRoom.find(id);
        if (roomIt == connectionRoom.end()) {
            return;  // a rejected/unrecognized connection - ignore whatever it sends
        }
        Room& room = *rooms.at(roomIt->second);

        const auto sessionIt = room.players.find(id);
        if (sessionIt == room.players.end()) {
            return;  // a spectator - clicks do nothing
        }

        const auto* click = std::get_if<ClickMessage>(&decoded);
        if (!click) {
            return;
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
                return;
            }
        }

        session.controller->handleCellClick(cell.row(), cell.col());
    });

    server.onDisconnect([&](const WsServerTransport::ConnectionId& id) {
        std::lock_guard<std::mutex> lock(gameMutex);
        pendingConnections.erase(id);
        const auto it = connectionRoom.find(id);
        if (it != connectionRoom.end()) {
            const auto roomIt = rooms.find(it->second);
            if (roomIt != rooms.end()) {
                roomIt->second->players.erase(id);
                roomIt->second->spectators.erase(id);
            }
            connectionRoom.erase(it);
        }
        std::cout << "[server] connection " << id << " disconnected" << std::endl;
    });

    server.start();
    std::cout << "[server] listening on port " << kPort << std::endl;

    while (true) {
        // Recipient ids must be copied out here too, not just the render state: room.players/
        // spectators are gameMutex-protected shared state, and a disconnect on another thread
        // can erase from them at any time. Reading them via broadcastToRoom(Room&, ...) after
        // releasing the lock (as the encode/send step below deliberately does, to avoid holding
        // gameMutex during network I/O) would be an unsynchronized concurrent read/write on the
        // same unordered_maps - undefined behavior, not just a stale-data risk.
        std::vector<std::pair<std::vector<std::string>, std::vector<RenderPiece>>> snapshots;
        {
            std::lock_guard<std::mutex> lock(gameMutex);
            for (auto& [key, roomPtr] : rooms) {
                (void)key;
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
                snapshots.emplace_back(std::move(recipients), roomPtr->game->getRenderState());
            }
        }
        for (const auto& [recipients, state] : snapshots) {
            const std::string text = encodeState(state);
            for (const auto& id : recipients) {
                server.send(id, text);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
    }
}
