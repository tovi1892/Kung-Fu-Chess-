#include "WsGateway.hpp"

#include <chrono>
#include <thread>
#include <utility>

namespace kungfu {

WsGateway::WsGateway(int listenPort, std::string upstreamUrl)
    : upstreamUrl_(std::move(upstreamUrl)), downstream_(listenPort) {}

void WsGateway::run() {
    downstream_.onConnect([this](const ConnectionId& id) { handleDownstreamConnect(id); });
    downstream_.onMessage(
        [this](const ConnectionId& id, const std::string& text) { handleDownstreamMessage(id, text); });
    downstream_.onDisconnect([this](const ConnectionId& id) { handleDownstreamDisconnect(id); });

    downstream_.start();

    // Nothing else to do on this thread - IXWebSocket runs the actual downstream listener and
    // every proxied upstream connection on their own background I/O thread(s); this call just
    // has to not return.
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(3600));
    }
}

void WsGateway::handleDownstreamConnect(const ConnectionId& id) {
    auto conn = std::make_shared<UpstreamConnection>();
    conn->transport = std::make_unique<net::WsClientTransport>(upstreamUrl_);

    conn->transport->onMessage([this, id](const std::string& text) { downstream_.send(id, text); });

    // Captures conn by weak_ptr, not shared_ptr: UpstreamConnection owns its WsClientTransport
    // (via unique_ptr), so a shared_ptr capture here would create a reference cycle
    // (transport -> its own onOpen_ std::function -> conn -> transport) that keeps
    // UpstreamConnection alive forever - meaning handleDownstreamDisconnect's erase() would
    // never actually destroy it, ~WsClientTransport() would never run, and the upstream
    // connection to the Shard would never close even after the real client disconnected.
    std::weak_ptr<UpstreamConnection> weakConn = conn;
    conn->transport->onOpen([this, weakConn]() {
        auto conn = weakConn.lock();
        if (!conn) {
            return;  // already torn down by handleDownstreamDisconnect
        }
        std::lock_guard<std::mutex> lock(mutex_);
        conn->open = true;
        for (const auto& pendingMsg : conn->pending) {
            conn->transport->send(pendingMsg);
        }
        conn->pending.clear();
    });
    conn->transport->start();

    std::lock_guard<std::mutex> lock(mutex_);
    upstreams_[id] = conn;
}

void WsGateway::handleDownstreamMessage(const ConnectionId& id, const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = upstreams_.find(id);
    if (it == upstreams_.end()) {
        return;
    }
    UpstreamConnection& conn = *it->second;
    if (conn.open) {
        conn.transport->send(text);
    } else {
        conn.pending.push_back(text);
    }
}

void WsGateway::handleDownstreamDisconnect(const ConnectionId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    upstreams_.erase(id);  // ~WsClientTransport stops the upstream connection
}

}  // namespace kungfu
