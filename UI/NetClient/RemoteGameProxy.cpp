#include "RemoteGameProxy.hpp"

#include "Network/Protocol.hpp"

namespace kungfu {

RemoteGameProxy::RemoteGameProxy(const std::string& serverUrl, const std::string& username)
    : username_(username), transport_(serverUrl) {
    transport_.onMessage([this](const std::string& text) { handleMessage(text); });
    transport_.onOpen([this]() { transport_.send(net::encodeJoin(username_)); });
    transport_.start();
}

void RemoteGameProxy::sendClick(int row, int col) {
    transport_.send(net::encodeClick(row, col));
}

void RemoteGameProxy::handleMessage(const std::string& text) {
    const auto decoded = net::decode(text);

    if (const auto* welcome = std::get_if<net::WelcomeMessage>(&decoded)) {
        std::lock_guard<std::mutex> lock(colorMutex_);
        myColor_ = welcome->color;
        hasColor_ = true;
    } else if (const auto* players = std::get_if<net::PlayersMessage>(&decoded)) {
        std::lock_guard<std::mutex> lock(playersMutex_);
        players_ = KnownPlayers{players->white, players->black};
    } else if (const auto* state = std::get_if<net::StateMessage>(&decoded)) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        latestState_ = state->pieces;
    } else if (const auto* move = std::get_if<MoveStarted>(&decoded)) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        pendingEvents_.push_back(*move);
    } else if (const auto* capture = std::get_if<PieceCaptured>(&decoded)) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        pendingEvents_.push_back(*capture);
    } else if (const auto* start = std::get_if<GameStarted>(&decoded)) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        pendingEvents_.push_back(*start);
    } else if (const auto* end = std::get_if<GameEnded>(&decoded)) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        pendingEvents_.push_back(*end);
    }
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

bool RemoteGameProxy::hasColor() const {
    std::lock_guard<std::mutex> lock(colorMutex_);
    return hasColor_;
}

PlayerColor RemoteGameProxy::myColor() const {
    std::lock_guard<std::mutex> lock(colorMutex_);
    return myColor_;
}

}  // namespace kungfu
