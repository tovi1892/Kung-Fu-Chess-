#include "Outbox.hpp"

namespace kungfu {

void Outbox::enqueue(const net::WsServerTransport::ConnectionId& id, const std::string& text) {
    pending_.emplace_back(id, text);
}

void Outbox::enqueueToRoom(const Room& room, const std::string& text) {
    for (const auto& [id, session] : room.players) {
        (void)session;
        pending_.emplace_back(id, text);
    }
    for (const auto& id : room.spectators) {
        pending_.emplace_back(id, text);
    }
}

void Outbox::flush() {
    std::vector<std::pair<net::WsServerTransport::ConnectionId, std::string>> outgoing;
    {
        std::lock_guard<std::mutex> lock(gameMutex_);
        outgoing.swap(pending_);
    }
    for (const auto& [id, text] : outgoing) {
        server_.send(id, text);
    }
}

}  // namespace kungfu
