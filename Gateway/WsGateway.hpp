#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Network/WsClientTransport.hpp"
#include "Network/WsServerTransport.hpp"

namespace kungfu {

// A transparent 1:1 WebSocket proxy: for every real client connection accepted on
// listenPort, opens exactly one upstream connection to upstreamUrl (today, the Shard -
// Server/main.cpp's kungfu_server, unchanged) and relays messages both directions
// byte-for-byte. Understands nothing about the game protocol - GameServer/RoomManager/etc.
// on the Shard side still see what looks like a real client connecting directly, so none of
// that domain logic needed to change for this split to exist.
//
// The hardcoded single upstreamUrl is deliberately the seam a future Game Allocator would
// plug into (routing each new connection to one of several shards instead of always the
// same one) - not built yet, this class only ever proxies to one fixed address.
//
// Accepted simplification: if the upstream (Shard) connection drops unexpectedly,
// WsClientTransport auto-reconnects on its own (existing IXWebSocket behavior) rather than
// this class proactively closing the downstream client - reasonable for a single always-up
// local Shard, not something to rely on once this sits in front of multiple shards.
class WsGateway {
public:
    using ConnectionId = net::WsServerTransport::ConnectionId;

    WsGateway(int listenPort, std::string upstreamUrl);

    // Starts the downstream listener and blocks forever - the last call main() makes.
    [[noreturn]] void run();

private:
    // One proxied client's upstream leg: the WsClientTransport connecting to the Shard, plus
    // enough state to buffer any downstream messages that arrive before that connection has
    // actually finished its own handshake (WsClientTransport::start() is asynchronous) -
    // otherwise a client's very first message (typically LOGIN, sent immediately after
    // connecting) could race the upstream socket still being closed and get silently
    // dropped. Held by shared_ptr rather than owned directly by upstreams_ so the onOpen
    // callback (fired on the upstream's own IXWebSocket thread) can safely keep it alive even
    // if handleDownstreamDisconnect erases the map entry concurrently.
    struct UpstreamConnection {
        std::unique_ptr<net::WsClientTransport> transport;
        bool open = false;
        std::vector<std::string> pending;
    };

    void handleDownstreamConnect(const ConnectionId& id);
    void handleDownstreamMessage(const ConnectionId& id, const std::string& text);
    void handleDownstreamDisconnect(const ConnectionId& id);

    std::string upstreamUrl_;
    net::WsServerTransport downstream_;

    // Guards upstreams_ - WsServerTransport's and WsClientTransport's callbacks each fire on
    // their own IXWebSocket background I/O thread(s), same contract as
    // Server/GameServer.hpp's registryMutex_.
    std::mutex mutex_;
    std::unordered_map<ConnectionId, std::shared_ptr<UpstreamConnection>> upstreams_;
};

}  // namespace kungfu
