#include "RemoteGameProxy.hpp"

#include "Network/Protocol.hpp"

namespace kungfu {

RemoteGameProxy::RemoteGameProxy(const std::string& serverUrl, const std::string& username, const std::string& password,
                                  net::JoinMode mode, const std::string& room)
    : username_(username), transport_(serverUrl) {
    transport_.onMessage([this](const std::string& text) { handleMessage(text); });
    transport_.onOpen([this, username, password, mode, room]() {
        pendingJoinMode_ = mode;
        pendingJoinRoom_ = room;
        transport_.send(net::encodeLogin(username, password));
    });
    transport_.start();
}

void RemoteGameProxy::sendClick(int row, int col) {
    transport_.send(net::encodeClick(row, col));
}

void RemoteGameProxy::handleMessage(const std::string& text) {
    std::visit(
        [this](const auto& msg) {
            using T = std::decay_t<decltype(msg)>;
            if constexpr (std::is_same_v<T, net::LoginOkMessage>) {
                {
                    std::lock_guard<std::mutex> lock(sessionMutex_);
                    loginResolved_ = true;
                    rating_ = msg.rating;
                }
                transport_.send(net::encodeJoin(pendingJoinMode_, pendingJoinRoom_));
            } else if constexpr (std::is_same_v<T, net::LoginFailMessage>) {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                loginResolved_ = true;
                loginFailed_ = true;
                loginFailureReason_ = msg.reason;
            } else if constexpr (std::is_same_v<T, net::WelcomeMessage>) {
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
            } else if constexpr (std::is_same_v<T, net::NoOpponentMessage>) {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                searchFailed_ = true;
            } else if constexpr (std::is_same_v<T, net::ForfeitWarningMessage>) {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                forfeitWarning_ = ForfeitWarning{msg.disconnectedColor,
                                                  std::chrono::steady_clock::now() +
                                                      std::chrono::milliseconds(msg.graceMs)};
            } else if constexpr (std::is_same_v<T, net::RatingsMessage>) {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                ratings_ = KnownRatings{msg.whiteRating, msg.blackRating};
                if (!isSpectator_) {
                    rating_ = myColor_ == PlayerColor::White ? msg.whiteRating : msg.blackRating;
                }
            } else if constexpr (std::is_same_v<T, net::ForfeitMessage>) {
                {
                    std::lock_guard<std::mutex> lock(sessionMutex_);
                    forfeitWarning_.reset();
                }
                std::lock_guard<std::mutex> lock(queueMutex_);
                pendingEvents_.push_back(msg);
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
                } else if constexpr (std::is_same_v<T, net::ForfeitMessage>) {
                    forfeitBus_.publish(e);
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

bool RemoteGameProxy::loginResolved() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return loginResolved_;
}

bool RemoteGameProxy::loginFailed() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return loginFailed_;
}

std::string RemoteGameProxy::loginFailureReason() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return loginFailureReason_;
}

int RemoteGameProxy::myRating() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return rating_;
}

KnownRatings RemoteGameProxy::ratings() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return ratings_;
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

bool RemoteGameProxy::searchFailed() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return searchFailed_;
}

std::optional<std::string> RemoteGameProxy::roomKey() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return roomKey_;
}

std::optional<ForfeitWarning> RemoteGameProxy::forfeitWarning() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return forfeitWarning_;
}

}  // namespace kungfu
