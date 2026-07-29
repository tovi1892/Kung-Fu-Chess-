#include "WsGateway.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>

#include "Network/MatchmakerProtocol.hpp"
#include "Network/Protocol.hpp"

namespace kungfu {

namespace {
constexpr auto kRedirectTimeout = std::chrono::seconds(5);

// Destroying a net::WsClientTransport blocks joining its own IXWebSocket I/O thread
// (~WsClientTransport -> stop()) - safe from a *different* thread (redirectAndJoin's old-leg
// teardown always is, since it's only ever called from the downstream thread or a different
// leg's callback thread), but destroying a transport *from within its own onMessage/onOpen
// callback* means that callback IS the thread stop() tries to join - pthread reports this as
// EDEADLK, which surfaces as std::thread throwing "Resource deadlock avoided" and crashing
// the process (confirmed empirically: this is exactly what happened before this helper
// existed). Deferring the actual destruction to a freshly spawned, detached thread sidesteps
// this unconditionally, regardless of which thread calls in - used anywhere a leg might be
// tearing down its own currently-executing connection.
void destroyTransportAsync(std::unique_ptr<net::WsClientTransport> transport) {
    if (!transport) {
        return;
    }
    std::thread([t = std::move(transport)]() mutable { t.reset(); }).detach();
}
}  // namespace

WsGateway::WsGateway(int listenPort, std::vector<std::string> shardUrls, std::string matchmakerUrl,
                      std::shared_ptr<IRoomRegistry> roomRegistry, net::Logger& logger)
    : shardUrls_(std::move(shardUrls)), matchmakerUrl_(std::move(matchmakerUrl)),
      roomRegistry_(std::move(roomRegistry)), logger_(logger), downstream_(listenPort) {
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
                decodeNeeded = (phase == JoinPhase::AwaitingJoin || phase == JoinPhase::RedirectAwaitingLoginReply ||
                                phase == JoinPhase::AwaitingRoomReply);
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
                        if (action == RoomReplyAction::Suppress) {
                            // QuickMatch's synthetic JoinRoom - the client-facing protocol
                            // never shows a ROOM message, so swallow this reply entirely.
                            logger_.log("quick-match room " + room->key + " ready on " + shardUrl);
                            suppress = true;
                        } else {
                            roomRegistry_->registerRoom(room->key, shardUrl);
                            logger_.log("freeform room '" + room->key + "' registered on " + shardUrl);
                        }
                    }
                }
            } else if (phase == JoinPhase::AwaitingJoin) {
                // Peek (never suppress) the shard's LOGIN_OK reply to learn this connection's
                // rating, needed to build a QuickMatch WAIT message later - forwarded to the
                // client completely unmodified either way.
                if (const auto* ok = std::get_if<net::LoginOkMessage>(&decoded)) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (generation == conn->generation) {
                        conn->cachedRating = ok->rating;
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

    if (phase == JoinPhase::AwaitingMatchmaker) {
        // The shard leg sits genuinely idle during a QuickMatch wait (never told this
        // connection exists) and could be pointing at a shard that won't even host the
        // eventual match - unlike every other phase past AwaitingJoin, sendOrQueue here
        // would silently misroute into the wrong place. Shouldn't happen per protocol (the
        // client has nothing to send while waiting), so log-and-drop rather than relay.
        logger_.log(id + " sent a message while awaiting a quick-match opponent - dropped");
        return;
    }

    if (phase != JoinPhase::AwaitingLogin && phase != JoinPhase::AwaitingJoin) {
        sendOrQueue(conn, text);
        return;
    }

    if (phase == JoinPhase::AwaitingLogin) {
        // Decode (don't require) the client's own username from its LOGIN text, needed to
        // build a QuickMatch WAIT message later - fail-open if it doesn't decode as LOGIN
        // (shouldn't happen per protocol; see the redirect-after-LOGIN-retry known limitation).
        const net::DecodedMessage decodedLogin = net::decode(text);
        if (const auto* login = std::get_if<net::LoginMessage>(&decodedLogin)) {
            std::lock_guard<std::mutex> lock(mutex_);
            conn->username = login->username;
        }
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

    if (join->mode == net::JoinMode::QuickMatch) {
        // Route through the Matchmaker instead of forwarding to the shard leg - the raw
        // client JOIN text must never reach a shard from here on for this connection (see
        // WsGateway.hpp's AwaitingMatchmaker doc and the Matchmaker feature's design notes:
        // reintroducing a "just forward it" fallback here would let the shard's own
        // still-present handleQuickMatch double-seat this connection into a second room).
        std::string username;
        int rating = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            conn->phase = JoinPhase::AwaitingMatchmaker;
            username = conn->username;
            rating = conn->cachedRating;
        }

        // Assign into conn->matchmakerTransport *before* calling start() - matching
        // redirectAndJoin's exact ordering for the shard leg, and for the same reason: once
        // start() begins, onOpen/onMessage can fire on another thread at any moment (even
        // near-instantly) and read conn->matchmakerTransport - it must never observe it still
        // null. Unlike handleDownstreamConnect's brand-new UpstreamConnection (nothing else
        // could reference it yet, so no lock is needed there), this conn is already live in
        // upstreams_, so the assignment itself needs the lock.
        auto mmTransport = std::make_unique<net::WsClientTransport>(matchmakerUrl_);
        net::WsClientTransport* rawTransport = mmTransport.get();
        int generation;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            conn->matchmakerTransport = std::move(mmTransport);
            generation = conn->matchmakerGeneration;
        }
        attachMatchmakerCallbacks(id, conn, *rawTransport, generation);
        rawTransport->start();
        sendOrMatchmakerQueue(conn, net::mm::encodeWait(username, rating));
        return;
    }

    // CreateRoom only reaches here (JoinRoom always returns above).
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn->phase = JoinPhase::AwaitingRoomReply;
        conn->pendingRoomReplyAction = RoomReplyAction::AddShardPrefix;
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
    // Async, not inline: failRedirect can be called from the shard leg's own callback
    // (LOGIN_FAIL on replay) - destroying that same leg inline would be the self-join
    // hazard destroyTransportAsync's own comment describes.
    destroyTransportAsync(std::move(transport));
}

void WsGateway::handleDownstreamDisconnect(const ConnectionId& id) {
    std::shared_ptr<UpstreamConnection> conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = upstreams_.find(id);
        if (it == upstreams_.end()) {
            return;
        }
        conn = std::move(it->second);
        upstreams_.erase(it);
    }
    // conn - and its transport/matchmakerTransport unique_ptrs - is destroyed here, once this
    // scope ends, with mutex_ already released. Same detach-then-destroy-after-unlock
    // discipline redirectAndJoin/failRedirect already use, for the same reason:
    // ~WsClientTransport blocks in stop(), which could deadlock against that same leg's own
    // in-flight callback trying to re-lock mutex_ if destroyed while still holding it. This is
    // also what triggers the Matchmaker's own onDisconnect cleanup for a connection that was
    // still searching when the client dropped - no explicit CANCEL message needed.
}

void WsGateway::attachMatchmakerCallbacks(const ConnectionId& id, std::weak_ptr<UpstreamConnection> weakConn,
                                           net::WsClientTransport& transport, int generation) {
    transport.onMessage([this, id, weakConn, generation](const std::string& text) {
        auto conn = weakConn.lock();
        if (!conn) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation != conn->matchmakerGeneration) {
                return;  // stale leg - already superseded/torn down, ignore
            }
        }

        const net::mm::DecodedMmMessage decoded = net::mm::decode(text);

        if (const auto* matched = std::get_if<net::mm::MatchedMessage>(&decoded)) {
            if (matched->shardIndex >= shardUrls_.size()) {
                // A SHARD_URLS/SHARD_COUNT deployment mismatch between this Gateway and the
                // Matchmaker - there's no wire message for this, a clean disconnect is the
                // honest alternative (same reasoning failRedirect's own doc comment gives).
                logger_.log(id + " matchmaker returned out-of-range shard index " +
                            std::to_string(matched->shardIndex));
                // Clear this leg *before* failRedirect, which may erase upstreams_' last
                // reference to conn - if that happens while conn->matchmakerTransport (this
                // very leg) is still set, ~UpstreamConnection would destroy it inline on this
                // same thread, the exact self-join hazard destroyTransportAsync exists to avoid.
                teardownMatchmakerLeg(conn);
                failRedirect(id, conn);
                return;
            }
            const std::string targetUrl = shardUrls_[matched->shardIndex];
            const std::string rewritten = net::encodeJoin(net::JoinMode::JoinRoom, matched->roomKey);

            std::string currentUrl;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                conn->pendingRoomReplyAction = RoomReplyAction::Suppress;
                currentUrl = conn->currentShardUrl;
            }
            if (targetUrl == currentUrl) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    conn->phase = JoinPhase::AwaitingRoomReply;
                }
                sendOrQueue(conn, rewritten);
            } else {
                redirectAndJoin(id, conn, targetUrl, rewritten);
            }
            teardownMatchmakerLeg(conn);
        } else if (std::holds_alternative<net::mm::NoOpponentMessage>(decoded)) {
            downstream_.send(id, net::encodeNoOpponent());
            {
                std::lock_guard<std::mutex> lock(mutex_);
                conn->phase = JoinPhase::Resolved;
            }
            teardownMatchmakerLeg(conn);
        }
        // Anything else (malformed/unexpected) is silently ignored, matching this file's
        // fail-open idiom elsewhere.
    });

    transport.onOpen([this, weakConn, generation]() {
        auto conn = weakConn.lock();
        if (!conn) {
            return;
        }
        std::vector<std::string> toFlush;
        net::WsClientTransport* t = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation != conn->matchmakerGeneration) {
                return;  // superseded before this handshake finished
            }
            conn->matchmakerOpen = true;
            toFlush = std::move(conn->matchmakerPending);
            conn->matchmakerPending.clear();
            t = conn->matchmakerTransport.get();
        }
        for (const auto& msg : toFlush) {
            t->send(msg);
        }
    });
}

void WsGateway::sendOrMatchmakerQueue(const std::shared_ptr<UpstreamConnection>& conn, const std::string& text) {
    net::WsClientTransport* t = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (conn->matchmakerOpen) {
            t = conn->matchmakerTransport.get();
        } else {
            conn->matchmakerPending.push_back(text);
        }
    }
    if (t != nullptr) {
        t->send(text);  // outside the lock - see sendOrQueue's comment above
    }
}

void WsGateway::teardownMatchmakerLeg(std::shared_ptr<UpstreamConnection> conn) {
    std::unique_ptr<net::WsClientTransport> old;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++conn->matchmakerGeneration;
        old = std::move(conn->matchmakerTransport);
        conn->matchmakerOpen = false;
        conn->matchmakerPending.clear();
    }
    // Async, not inline: this is always called from within the matchmaker leg's own
    // onMessage callback (MATCHED/NO_OPPONENT handling) - destroying that same leg inline
    // would try to join the very thread running this code (self-join), which crashes the
    // process (see destroyTransportAsync's comment - confirmed empirically before this fix).
    destroyTransportAsync(std::move(old));
}

}  // namespace kungfu
