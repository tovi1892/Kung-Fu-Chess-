#include "RoomManager.hpp"

#include <algorithm>
#include <cmath>

namespace kungfu {

Room& RoomManager::getOrCreateRoom(const std::string& key, const std::function<void(Room&)>& onCreated) {
    auto it = rooms_.find(key);
    if (it == rooms_.end()) {
        auto room = createRoom(key);
        if (onCreated) {
            onCreated(*room);
        }
        it = rooms_.emplace(key, std::move(room)).first;
    }
    return *it->second;
}

std::unordered_map<std::string, std::unique_ptr<Room>>& RoomManager::rooms() {
    return rooms_;
}

std::string RoomManager::allocateNamedRoomKey() {
    return std::to_string(nextNamedRoomId_++);
}

std::string RoomManager::allocateQuickMatchRoomKey() {
    return "quickmatch-" + std::to_string(nextQuickMatchId_++);
}

std::optional<WaitingPlayer> RoomManager::matchWaiter(int rating, int eloRange) {
    const auto it = std::find_if(quickMatchWaiting_.begin(), quickMatchWaiting_.end(), [&](const WaitingPlayer& w) {
        return std::abs(w.rating - rating) <= eloRange;
    });
    if (it == quickMatchWaiting_.end()) {
        return std::nullopt;
    }
    const WaitingPlayer matched = *it;
    quickMatchWaiting_.erase(it);
    return matched;
}

void RoomManager::addWaiter(WaitingPlayer waiter) {
    quickMatchWaiting_.push_back(std::move(waiter));
}

void RoomManager::removeWaiter(const std::string& connectionId) {
    quickMatchWaiting_.erase(std::remove_if(quickMatchWaiting_.begin(), quickMatchWaiting_.end(),
                                             [&](const WaitingPlayer& w) { return w.id == connectionId; }),
                              quickMatchWaiting_.end());
}

std::vector<WaitingPlayer> RoomManager::reapExpiredWaiters(std::chrono::steady_clock::time_point now) {
    std::vector<WaitingPlayer> expired;
    for (auto it = quickMatchWaiting_.begin(); it != quickMatchWaiting_.end();) {
        if (now >= it->deadline) {
            expired.push_back(*it);
            it = quickMatchWaiting_.erase(it);
        } else {
            ++it;
        }
    }
    return expired;
}

std::optional<RoomManager::ReconnectResult> RoomManager::reconnect(const std::string& username,
                                                                    const std::string& newConnectionId) {
    for (auto& [roomKey, roomPtr] : rooms_) {
        std::unique_lock<std::mutex> lock(roomPtr->roomMutex);

        if (!roomPtr->pendingForfeit.has_value()) {
            continue;  // lock released here - next iteration takes a fresh room's lock
        }
        const auto sessionIt = std::find_if(roomPtr->players.begin(), roomPtr->players.end(), [&](const auto& entry) {
            return entry.second.username == username &&
                   entry.second.color == roomPtr->pendingForfeit->disconnectedColor;
        });
        if (sessionIt == roomPtr->players.end()) {
            continue;
        }

        PlayerSession session = std::move(sessionIt->second);
        roomPtr->players.erase(sessionIt);
        // Clears whatever was selected at the moment of disconnect (attachGame's documented
        // side effect) - re-attaching the same game it already had, purely for that effect.
        session.controller->attachGame(session.controller->game());
        const PlayerColor reconnectedColor = session.color;
        roomPtr->players[newConnectionId] = std::move(session);
        roomPtr->pendingForfeit.reset();

        return ReconnectResult{roomPtr.get(), reconnectedColor, std::move(lock)};
    }
    return std::nullopt;
}

void RoomManager::assignConnectionToRoom(const std::string& connectionId, const std::string& roomKey) {
    connectionRoom_[connectionId] = roomKey;
}

bool RoomManager::isConnectionRouted(const std::string& connectionId) const {
    return connectionRoom_.count(connectionId) > 0;
}

Room* RoomManager::roomForConnection(const std::string& connectionId) const {
    const auto it = connectionRoom_.find(connectionId);
    if (it == connectionRoom_.end()) {
        return nullptr;
    }
    return rooms_.at(it->second).get();
}

void RoomManager::forgetConnectionRoom(const std::string& connectionId) {
    connectionRoom_.erase(connectionId);
}

void RoomManager::startForfeitCountdown(Room& room, PlayerColor disconnectedColor, int graceMs) {
    room.pendingForfeit =
        PendingForfeit{disconnectedColor, std::chrono::steady_clock::now() + std::chrono::milliseconds(graceMs)};
}

std::optional<PlayerColor> RoomManager::resolveForfeitIfExpired(Room& room, std::chrono::steady_clock::time_point now) {
    if (!room.pendingForfeit.has_value() || now < room.pendingForfeit->deadline) {
        return std::nullopt;
    }

    const PlayerColor winner =
        room.pendingForfeit->disconnectedColor == PlayerColor::White ? PlayerColor::Black : PlayerColor::White;

    // Move everyone still tracked into spectators (keeps receiving STATE) and stop the
    // simulation - GameEngine itself has no idea a forfeit happened (no king was
    // captured), so without this it would happily keep running and could later fire a
    // second, conflicting GameEnded.
    for (const auto& [playerId, session] : room.players) {
        (void)session;
        room.spectators.insert(playerId);
    }
    room.players.clear();
    room.game->stop();
    room.pendingForfeit.reset();

    return winner;
}

}  // namespace kungfu
