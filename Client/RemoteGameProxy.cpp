#include "RemoteGameProxy.hpp"

#include "Network/Protocol.hpp"

namespace kungfu {

RemoteGameProxy::RemoteGameProxy(const std::string& serverUrl, const std::string& username, net::JoinMode mode,
                                  const std::string& room)
    : username_(username), transport_(serverUrl) {
    transport_.onMessage([this](const std::string& text) { handleMessage(text); });
    transport_.onOpen([this, mode, room]() { transport_.send(net::encodeJoin(username_, mode, room)); });
    transport_.start();
}

void RemoteGameProxy::sendClick(int row, int col) {
    transport_.send(net::encodeClick(row, col));
}

void RemoteGameProxy::handleMessage(const std::string& text) {
    std::visit(
        [this](const auto& msg) {
            using T = std::decay_t<decltype(msg)>;
            if constexpr (std::is_same_v<T, net::WelcomeMessage>) {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                myColor_ = msg.color;
                hasColor_ = true;
            } else if constexpr (std::is_same_v<T, net::SpectateMessage>) {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                isSpectator_ = true;
            } else if constexpr (std::is_same_v<T, net::RoomMessage>) {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                roomKey_ = msg.key;
            } else if constexpr (std::is_same_v<T, net::PlayersMessage>) {
                std::lock_guard<std::mutex> lock(playersMutex_);
                players_ = KnownPlayers{msg.white, msg.black};
            } else if constexpr (std::is_same_v<T, net::StateMessage>) {
                std::lock_guard<std::mutex> lock(stateMutex_);
                latestState_ = msg.pieces;
            } else if constexpr (std::is_same_v<T, MoveStarted> || std::is_same_v<T, PieceCaptured> ||
                                  std::is_same_v<T, GameStarted> || std::is_same_v<T, GameEnded>) {
                std::lock_guard<std::mutex> lock(queueMutex_);
                pendingEvents_.push_back(msg);
            }
 
        },
        net::decode(text));
}

void RemoteGameProxy::pollEvents() {
    std::vector<QueuedEvent> events;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        events.swap(pendingEvents_);
    }

    for (const auto& event : events) {
        std::visit(
            [this](const auto& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, MoveStarted>) {
                    moveBus_.publish(e);
                } else if constexpr (std::is_same_v<T, PieceCaptured>) {
                    captureBus_.publish(e);
                } else if constexpr (std::is_same_v<T, GameStarted>) {
                    startBus_.publish(e);
                } else if constexpr (std::is_same_v<T, GameEnded>) {
                    endBus_.publish(e);
                }
            },
            event);
    }
}

std::vector<RenderPiece> RemoteGameProxy::getRenderState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return latestState_;
}

KnownPlayers RemoteGameProxy::players() const {
    std::lock_guard<std::mutex> lock(playersMutex_);
    return players_;
}

bool RemoteGameProxy::hasRole() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return hasColor_ || isSpectator_;
}

bool RemoteGameProxy::isSpectator() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return isSpectator_;
}

PlayerColor RemoteGameProxy::myColor() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return myColor_;
}

std::optional<std::string> RemoteGameProxy::roomKey() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return roomKey_;
}

}  // namespace kungfu
