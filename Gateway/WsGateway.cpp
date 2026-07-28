#include "WsGateway.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>

#include "Network/Protocol.hpp"

namespace kungfu {

namespace {
constexpr auto kRedirectTimeout = std::chrono::seconds(5);
}  // namespace

WsGateway::WsGateway(int listenPort, std::vector<std::string> shardUrls,
                      std::shared_ptr<IRoomRegistry> roomRegistry, net::Logger& logger)
    : shardUrls_(std::move(shardUrls)), roomRegistry_(std::move(roomRegistry)), logger_(logger),
      downstream_(listenPort) {
    if (shardUrls_.empty()) {
        throw std::invalid_argument("WsGateway requires at least one shard URL");
    }
}

std::string WsGateway::pickShardUrl() {
    const std::size_t index = nextShardIndex_.fetch_add(1, std::memory_order_relaxed) % shardUrls_.size();
    return shardUrls_[index];
}

std::optional<std::pair<std::size_t, std::string>> WsGateway::parsePrefixedKey(const std::string& roomKey) const {
    const auto colon = roomKey.find(':');
    if (colon == std::string::npos || colon == 0) {
        return std::nullopt;
    }
    const std::string indexPart = roomKey.substr(0, colon);
    for (char c : indexPart) {
        if (c < '0' || c > '9') {
            return std::nullopt;  // not a plain non-negative integer - a freeform key
        }
    }
    std::size_t index;
    try {
        index = static_cast<std::size_t>(std::stoul(indexPart));
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (index >= shardUrls_.size()) {
        return std::nullopt;  // out of range - treat as freeform rather than misroute
    }
    return std::make_pair(index, roomKey.substr(colon + 1));
}

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

void WsGateway::attachUpstreamCallbacks(const ConnectionId& id, std::weak_ptr<UpstreamConnection> weakConn,
                                         net::WsClientTransport& transport, int generation) {
    transport.onMessage([this, id, weakConn, generation](const std::string& text) {
        auto conn = weakConn.lock();
        if (!conn) {
            return;
        }

        bool decodeNeeded = false;
        bool suppress = false;
        JoinPhase phase = JoinPhase::AwaitingLogin;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation == conn->generation) {
                phase = conn->phase;
                decodeNeeded =
                    (phase == JoinPhase::RedirectAwaitingLoginReply || phase == JoinPhase::AwaitingRoomReply);
            }
            // generation mismatch: a stale leg's straggler message - forward untouched below,
            // with zero interpretation (see WsGateway.hpp's UpstreamConnection::generation doc).
        }

        if (decodeNeeded) {
            const net::DecodedMessage decoded = net::decode(text);

            if (phase == JoinPhase::RedirectAwaitingLoginReply &&
                (std::holds_alternative<net::LoginOkMessage>(decoded) ||
                 std::holds_alternative<net::LoginFailMessage>(decoded))) {
                if (std::holds_alternative<net::LoginFailMessage>(decoded)) {
                    std::string failedShardUrl;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        failedShardUrl = conn->currentShardUrl;
                    }
                    logger_.log("redirect for " + id + " failed: replayed LOGIN rejected by " + failedShardUrl);
                    failRedirect(id, conn);
                    return;
                }
                std::string joinText;
                net::WsClientTransport* t = nullptr;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (generation == conn->generation) {
                        conn->phase = JoinPhase::AwaitingRoomReply;
                        joinText = conn->pendingJoinAfterReplay;
                        conn->pendingJoinAfterReplay.clear();
                        t = conn->transport.get();
                    }
                }
                if (t != nullptr) {
                    t->send(joinText);  // sent outside the lock - see sendOrQueue's comment
                }
                suppress = true;  // swallow the replayed LOGIN_OK itself - the client already
                                  // got a real one from the original shard
            } else if (phase == JoinPhase::AwaitingRoomReply) {
                if (const auto* room = std::get_if<net::RoomMessage>(&decoded)) {
                    std::string shardUrl;
                    RoomReplyAction action = RoomReplyAction::AddShardPrefix;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (generation == conn->generation) {
                            conn->phase = JoinPhase::Resolved;
                            shardUrl = conn->currentShardUrl;
                            action = conn->pendingRoomReplyAction;
                        }
                    }
                    if (!shardUrl.empty()) {
                        if (action == RoomReplyAction::AddShardPrefix) {
                            std::size_t shardIndex = 0;
                            for (std::size_t i = 0; i < shardUrls_.size(); ++i) {
                                if (shardUrls_[i] == shardUrl) {
                                    shardIndex = i;
                                    break;
                                }
                            }
                            const std::string prefixedKey = std::to_string(shardIndex) + ":" + room->key;
                            downstream_.send(id, net::encodeRoom(prefixedKey));
                            logger_.log("room " + prefixedKey + " ready on " + shardUrl);
                            return;  // already forwarded the rewritten text - don't fall through
                        }
                        roomRegistry_->registerRoom(room->key, shardUrl);
                        logger_.log("freeform room '" + room->key + "' registered on " + shardUrl);
                    }
                }
            }
        }

        if (!suppress) {
            downstream_.send(id, text);
        }
    });

    transport.onOpen([this, id, weakConn, generation]() {
        auto conn = weakConn.lock();
        if (!conn) {
            return;
        }
        std::vector<std::string> toFlush;
        net::WsClientTransport* t = nullptr;
        bool isRedirect = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation != conn->generation) {
                return;  // superseded by a later redirect before this one's handshake finished
            }
            conn->open = true;
            toFlush = std::move(conn->pending);
            conn->pending.clear();
            t = conn->transport.get();
            isRedirect = (conn->phase == JoinPhase::AwaitingJoin && !conn->pendingJoinAfterReplay.empty());
        }
        for (const auto& msg : toFlush) {
            t->send(msg);
        }

        if (isRedirect) {
            std::string loginText;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (generation != conn->generation) {
                    return;
                }
                conn->phase = JoinPhase::RedirectAwaitingLoginReply;
                loginText = conn->cachedLoginText;
            }
            t->send(loginText);
        }
    });
}

void WsGateway::handleDownstreamConnect(const ConnectionId& id) {
    auto conn = std::make_shared<UpstreamConnection>();
    const std::string shardUrl = pickShardUrl();
    conn->currentShardUrl = shardUrl;
    conn->transport = std::make_unique<net::WsClientTransport>(shardUrl);

    attachUpstreamCallbacks(id, conn, *conn->transport, conn->generation);
    conn->transport->start();

    std::lock_guard<std::mutex> lock(mutex_);
    upstreams_[id] = conn;
    logger_.log(id + " connected, assigned shard " + shardUrl);
}

void WsGateway::sendOrQueue(const std::shared_ptr<UpstreamConnection>& conn, const std::string& text) {
    net::WsClientTransport* t = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (conn->open) {
            t = conn->transport.get();
        } else {
            conn->pending.push_back(text);
        }
    }
    if (t != nullptr) {
        t->send(text);  // outside the lock - WsClientTransport::send ultimately calls into
                         // IXWebSocket, which can synchronously reenter a callback on this
                         // same thread if the peer already closed (see
                         // Network/WsServerTransport.cpp's identical note); holding mutex_
                         // across that call risks a self-deadlock on this non-recursive mutex.
    }
}

void WsGateway::handleDownstreamMessage(const ConnectionId& id, const std::string& text) {
    std::shared_ptr<UpstreamConnection> conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = upstreams_.find(id);
        if (it == upstreams_.end()) {
            return;
        }
        conn = it->second;
    }

    JoinPhase phase;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        phase = conn->phase;
    }

    if (phase != JoinPhase::AwaitingLogin && phase != JoinPhase::AwaitingJoin) {
        sendOrQueue(conn, text);
        return;
    }

    if (phase == JoinPhase::AwaitingLogin) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            conn->cachedLoginText = text;
            conn->phase = JoinPhase::AwaitingJoin;
        }
        sendOrQueue(conn, text);
        return;
    }

    // phase == AwaitingJoin
    const net::DecodedMessage decoded = net::decode(text);
    const auto* join = std::get_if<net::JoinMessage>(&decoded);
    if (join == nullptr) {
        sendOrQueue(conn, text);  // shouldn't happen per protocol, fail open
        return;
    }

    if (join->mode == net::JoinMode::JoinRoom) {
        std::string currentUrl;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentUrl = conn->currentShardUrl;
        }

        if (const auto parsed = parsePrefixedKey(join->room)) {
            const auto& [shardIndex, rawKey] = *parsed;
            const std::string targetUrl = shardUrls_[shardIndex];
            const std::string rewritten = net::encodeJoin(net::JoinMode::JoinRoom, rawKey);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                conn->pendingRoomReplyAction = RoomReplyAction::AddShardPrefix;
            }
            if (targetUrl != currentUrl) {
                redirectAndJoin(id, conn, targetUrl, rewritten);
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                conn->phase = JoinPhase::AwaitingRoomReply;
            }
            sendOrQueue(conn, rewritten);
            return;
        }

        // Freeform key - consult the registry.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            conn->pendingRoomReplyAction = RoomReplyAction::RegisterFreeform;
        }
        const auto target = roomRegistry_->lookup(join->room);
        if (target.has_value() && *target != currentUrl) {
            redirectAndJoin(id, conn, *target, text);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            conn->phase = JoinPhase::AwaitingRoomReply;
        }
        sendOrQueue(conn, text);
        return;
    }

    const JoinPhase nextPhase = join->mode == net::JoinMode::QuickMatch ? JoinPhase::Resolved
                                                                         : JoinPhase::AwaitingRoomReply;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn->phase = nextPhase;
        conn->pendingRoomReplyAction = RoomReplyAction::AddShardPrefix;  // CreateRoom only reaches
                                                                          // here (JoinRoom always
                                                                          // returns above); QuickMatch
                                                                          // never gets a ROOM reply
    }
    sendOrQueue(conn, text);
}

void WsGateway::redirectAndJoin(const ConnectionId& id, std::shared_ptr<UpstreamConnection> conn,
                                 const std::string& newShardUrl, const std::string& originalJoinText) {
    auto newTransport = std::make_unique<net::WsClientTransport>(newShardUrl);
    int newGeneration;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_.log(id + " redirecting from " + conn->currentShardUrl + " to " + newShardUrl);
        newGeneration = conn->generation + 1;
        conn->pendingJoinAfterReplay = originalJoinText;
        conn->phase = JoinPhase::AwaitingJoin;  // becomes RedirectAwaitingLoginReply once
                                                 // this leg's onOpen fires and replays LOGIN
    }
    attachUpstreamCallbacks(id, conn, *newTransport, newGeneration);

    std::unique_ptr<net::WsClientTransport> oldTransport;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        oldTransport = std::move(conn->transport);  // detach, don't destroy under the lock -
                                                     // see below for why
        conn->transport = std::move(newTransport);
        conn->currentShardUrl = newShardUrl;
        conn->open = false;
        conn->generation = newGeneration;
    }
    conn->transport->start();
    // oldTransport's destructor (~WsClientTransport -> blocking stop()) runs here, once this
    // scope ends, with mutex_ already released. Destroying it *while holding mutex_* would
    // risk deadlock: if the old leg's own onMessage callback is concurrently trying to lock
    // mutex_ on its own IXWebSocket thread at that exact moment, that thread blocks on the
    // lock while this thread blocks inside stop() waiting for that same thread to exit.

    std::thread([this, id, conn, newGeneration]() {
        std::this_thread::sleep_for(kRedirectTimeout);
        bool timedOut = false;
        std::string shardUrl;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            timedOut = (conn->generation == newGeneration && conn->phase != JoinPhase::AwaitingRoomReply &&
                        conn->phase != JoinPhase::Resolved);
            shardUrl = conn->currentShardUrl;
        }
        if (timedOut) {
            logger_.log(id + " redirect to " + shardUrl + " timed out");
            failRedirect(id, conn);
        }
    }).detach();
}

void WsGateway::failRedirect(const ConnectionId& id, const std::shared_ptr<UpstreamConnection>& conn) {
    std::unique_ptr<net::WsClientTransport> transport;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = upstreams_.find(id);
        if (it == upstreams_.end() || it->second != conn) {
            return;  // already cleaned up (e.g. client disconnected first)
        }
        transport = std::move(conn->transport);
        upstreams_.erase(it);
    }
    downstream_.close(id);
    // transport's destructor runs here, lock already released - same reasoning as
    // redirectAndJoin's old-leg teardown.
}

void WsGateway::handleDownstreamDisconnect(const ConnectionId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    upstreams_.erase(id);  // ~WsClientTransport stops the upstream connection
}

}  // namespace kungfu
