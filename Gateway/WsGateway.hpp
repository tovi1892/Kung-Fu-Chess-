#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "IRoomRegistry.hpp"
#include "Network/Logger.hpp"
#include "Network/WsClientTransport.hpp"
#include "Network/WsServerTransport.hpp"

namespace kungfu {

// A WebSocket proxy in front of a set of Shards (kungfu_server instances) - understands just
// enough of the game protocol (LOGIN/JOIN/ROOM, never CLICK/STATE/move events) to route each
// connection to the right shard, while staying otherwise transparent: GameServer/RoomManager
// on the Shard side see a real client connecting directly, no domain logic there needed to
// change for this to exist.
//
// Routing summary (see WsGateway.cpp for the full state machine):
//  - A new downstream connection is assigned a shard round-robin, for LOGIN.
//  - CreateRoom's server-generated room key is rewritten on the way to the client to carry a
//    "<shardIndex>:" prefix - this is the only routing info a JoinRoom needs later, and it
//    means the common "create, share the code, friend joins" flow needs no external lookup at
//    all, and can never collide across shards (RoomManager::allocateNamedRoomKey is only
//    unique per-process, per Server/RoomManager.hpp).
//  - JoinRoom with a prefixed key routes by parsing the prefix; JoinRoom with a freeform
//    (client-invented, unprefixed) key falls back to an IRoomRegistry lookup (see
//    IRoomRegistry.hpp) - registered the first time such a key's ROOM reply is observed.
//  - If routing decides a connection needs a *different* shard than the one already handling
//    its LOGIN, the Gateway silently swaps upstream legs: opens a new connection to the
//    target shard, replays the client's original LOGIN there (safe because the account DB is
//    shared across shards - see Server_Design.md), swallows the replay's LOGIN_OK/FAIL reply
//    (the client already got a real one), then forwards the client's JOIN. All invisible to
//    the client, which only ever sees one continuous connection to the Gateway.
//  - Once a connection's JOIN is fully resolved, the Gateway stops decoding entirely for that
//    connection's remaining lifetime - CLICK/STATE traffic is pure high-throughput byte relay,
//    exactly like before this routing logic existed.
class WsGateway {
public:
    using ConnectionId = net::WsServerTransport::ConnectionId;

    // Throws std::invalid_argument if shardUrls is empty. logger must outlive this object -
    // same by-reference-injection contract as Server/GameServer.hpp's constructor.
    WsGateway(int listenPort, std::vector<std::string> shardUrls, std::string matchmakerUrl,
              std::shared_ptr<IRoomRegistry> roomRegistry, net::Logger& logger);

    // Starts the downstream listener and blocks forever - the last call main() makes.
    [[noreturn]] void run();

private:
    // How far a connection has progressed through LOGIN/JOIN - see WsGateway.cpp for exactly
    // which messages are decoded/relayed in each phase. Kept as one explicit state machine
    // rather than several independent booleans because the two directions (client->shard,
    // shard->client) need to stop decoding at different points in the sequence, and ad hoc
    // flags are where "decode gate never gets cleared" bugs live.
    enum class JoinPhase {
        AwaitingLogin,               // haven't seen this connection's LOGIN yet
        AwaitingJoin,                // LOGIN forwarded, haven't seen/forwarded JOIN yet
        RedirectAwaitingLoginReply,  // mid-redirect: replayed LOGIN sent on the new leg,
                                     // waiting for (and about to swallow) its LOGIN_OK/FAIL
        AwaitingMatchmaker,          // QuickMatch: WAIT sent to the Matchmaker leg, waiting
                                     // for MATCHED/NO_OPPONENT - the shard leg sits idle
                                     // throughout (see WsGateway.cpp's dispatch comment)
        AwaitingRoomReply,           // JOIN forwarded (CreateRoom/JoinRoom, or the synthetic
                                     // JoinRoom a Matchmaker match produces) - not yet observed
        Resolved,                    // pure relay forever after this, both directions
    };

    // What to do with the ROOM reply this connection is waiting on, set in
    // handleDownstreamMessage when the JOIN is forwarded/redirected and consumed once the
    // ROOM reply is observed. Needed because the ROOM key's own text can't reliably tell
    // CreateRoom apart from a freeform JoinRoom (a freeform key can be any string, including
    // one that happens to look like a plain, unprefixed CreateRoom key).
    enum class RoomReplyAction {
        AddShardPrefix,    // CreateRoom, or JoinRoom with an already-prefixed key - either
                           // way, prefixing the reply with (a possibly re-derived) current
                           // shard index is the correct, idempotent thing to do
        RegisterFreeform,  // JoinRoom with a freeform key - register it in roomRegistry_,
                           // forward the reply unmodified
        Suppress,          // QuickMatch's synthetic JoinRoom - the client-facing QuickMatch
                           // protocol never shows a ROOM message (see Network/Protocol.hpp),
                           // so this reply must never reach the client at all
    };

    // One proxied client's upstream leg, plus enough state to buffer any downstream messages
    // that arrive before the current upstream connection has actually finished its own
    // handshake (WsClientTransport::start() is asynchronous), and to support silently
    // swapping to a different shard mid-session (see redirectAndJoin). Held by shared_ptr
    // rather than owned directly by upstreams_ so callbacks fired on IXWebSocket's own
    // thread(s) can safely keep it alive even if handleDownstreamDisconnect erases the map
    // entry concurrently.
    struct UpstreamConnection {
        std::unique_ptr<net::WsClientTransport> transport;
        bool open = false;
        std::vector<std::string> pending;

        std::string currentShardUrl;
        std::string cachedLoginText;         // raw first message, replayed verbatim on redirect
        std::string pendingJoinAfterReplay;  // client's (possibly prefix-stripped) JOIN text,
                                              // held only during a redirect
        int generation = 0;                  // bumped on every leg swap - lets a stale leg's
                                              // callback recognize itself and stop interpreting
        JoinPhase phase = JoinPhase::AwaitingLogin;
        RoomReplyAction pendingRoomReplyAction = RoomReplyAction::AddShardPrefix;

        // Captured once, purely to build a QuickMatch WAIT message: username from the
        // client's own cached LOGIN text, cachedRating from the shard's LOGIN_OK reply
        // (peeked while forwarding it unmodified - see attachUpstreamCallbacks).
        std::string username;
        int cachedRating = 0;

        // The Matchmaker leg - genuinely parallel to, not a replacement for, the shard leg
        // above (transport/generation exist to swap *which shard* one connection points to;
        // this leg coexists alongside it during a QuickMatch wait). Same
        // open/pending-buffer/generation shape as the shard leg, scoped to just this leg.
        std::unique_ptr<net::WsClientTransport> matchmakerTransport;
        bool matchmakerOpen = false;
        std::vector<std::string> matchmakerPending;
        int matchmakerGeneration = 0;
    };

    void handleDownstreamConnect(const ConnectionId& id);
    void handleDownstreamMessage(const ConnectionId& id, const std::string& text);
    void handleDownstreamDisconnect(const ConnectionId& id);

    // Wires onMessage/onOpen for one upstream transport - shared between the initial connect
    // and any later redirect, since both need identical handling.
    void attachUpstreamCallbacks(const ConnectionId& id, std::weak_ptr<UpstreamConnection> weakConn,
                                  net::WsClientTransport& transport, int generation);

    // Silently moves an already-LOGIN'd connection to a different shard: opens a new upstream
    // leg, replays the cached LOGIN there, and (once that succeeds) forwards originalJoinText
    // - see WsGateway.cpp for the full sequencing and why the old leg must be torn down after
    // releasing mutex_, not while holding it.
    void redirectAndJoin(const ConnectionId& id, std::shared_ptr<UpstreamConnection> conn,
                          const std::string& newShardUrl, const std::string& originalJoinText);

    // Logs, tears down both upstream legs (if any), and closes the downstream connection -
    // used when a redirect can't complete (LOGIN_FAIL on replay, or the target shard never
    // finishes its handshake within the timeout). There's no wire message for "shard
    // unreachable, please retry", and falling back to the connection's original shard would
    // let RoomManager::getOrCreateRoom silently fabricate a wrong, empty room there instead of
    // actually joining the target game - a clean disconnect is the honest alternative.
    void failRedirect(const ConnectionId& id, const std::shared_ptr<UpstreamConnection>& conn);

    // Sends now if the upstream leg is already open, otherwise queues for the onOpen flush.
    // Never calls WsClientTransport::send() while holding mutex_ - see WsGateway.cpp's
    // concurrency comment on why that would risk deadlocking against IXWebSocket's documented
    // synchronous-reentrancy behavior (Network/WsServerTransport.cpp has the same note).
    void sendOrQueue(const std::shared_ptr<UpstreamConnection>& conn, const std::string& text);

    // Same role as sendOrQueue/attachUpstreamCallbacks above, but for the Matchmaker leg -
    // kept as separate methods rather than generalizing over "some WsClientTransport this
    // connection depends on", since the message shapes (net::mm vs net::) and the lack of any
    // redirect-this-leg flow make the two genuinely different, not just parameterized copies.
    void attachMatchmakerCallbacks(const ConnectionId& id, std::weak_ptr<UpstreamConnection> weakConn,
                                    net::WsClientTransport& transport, int generation);
    void sendOrMatchmakerQueue(const std::shared_ptr<UpstreamConnection>& conn, const std::string& text);

    // Detaches and tears down conn's matchmaker leg (if any), bumping matchmakerGeneration
    // first so a straggler callback on the old leg recognizes itself as stale - same
    // detach-under-lock/destroy-after-unlock discipline redirectAndJoin uses for the shard
    // leg, for the same reason (~WsClientTransport blocks in stop(), which could deadlock
    // against that same leg's own in-flight callback if destroyed while still holding mutex_).
    void teardownMatchmakerLeg(std::shared_ptr<UpstreamConnection> conn);

    std::string pickShardUrl();

    // roomKey must not contain the Gateway's own "<index>:" prefix syntax - parses a leading
    // "<digits>:" and validates the index is in range. Returns nullopt if roomKey doesn't
    // start with a valid shard index (i.e. it's a freeform, client-invented key).
    std::optional<std::pair<std::size_t, std::string>> parsePrefixedKey(const std::string& roomKey) const;

    std::vector<std::string> shardUrls_;
    std::string matchmakerUrl_;
    std::shared_ptr<IRoomRegistry> roomRegistry_;
    std::atomic<std::size_t> nextShardIndex_{0};
    net::Logger& logger_;

    net::WsServerTransport downstream_;

    // Guards upstreams_ and every UpstreamConnection's fields - WsServerTransport's and every
    // WsClientTransport's callbacks each fire on their own IXWebSocket background I/O
    // thread(s), same contract as Server/GameServer.hpp's registryMutex_.
    std::mutex mutex_;
    std::unordered_map<ConnectionId, std::shared_ptr<UpstreamConnection>> upstreams_;
};

}  // namespace kungfu
