#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "RedisWaitQueue.hpp"

#include "Network/Logger.hpp"
#include "Network/WsServerTransport.hpp"

namespace kungfu {

// Standalone cross-shard QuickMatch pairing service - the fix for Server/RoomManager.hpp's
// quickMatchWaiting_ being an in-memory list owned by one shard process, meaning two players
// who land on different shards (behind the WS Gateway) could never be paired. This service
// owns a Redis-backed waiting queue (see RedisWaitQueue.hpp) shared across every shard; the WS
// Gateway is the client here (exactly symmetric to Gateway->Shard) - see Gateway/WsGateway.hpp
// for the other half of this feature.
//
// Deliberately scoped to exactly one Matchmaker instance, matching the precedent
// Gateway/InMemoryRoomRegistry.hpp already set for the analogous Gateway-side limit: Redis
// holds the durable, inspectable queue data, but the pairing decision itself is only made
// atomic by this single process's own mutex_, not a Redis transaction/Lua script. Multiple
// Matchmaker replicas racing on the same queue is a known, deferred gap, not solved here.
class Matchmaker {
public:
    using ConnectionId = net::WsServerTransport::ConnectionId;

    // shardCount, not shard URLs - this service never dials a shard itself, it only ever
    // hands back a round-robin index for each Gateway to map against its own locally-
    // configured shard list. logger must outlive this object, same contract as WsGateway's.
    Matchmaker(int listenPort, int shardCount, int quickMatchTimeoutMs, std::shared_ptr<RedisWaitQueue> queue,
               net::Logger& logger);
    ~Matchmaker();

    // Starts the downstream listener and the timeout-sweep thread, then blocks forever.
    [[noreturn]] void run();

private:
    void handleMessage(const ConnectionId& id, const std::string& text);
    void handleDisconnect(const ConnectionId& id);

    // The only pairing-check trigger point: fires exactly once, when a *new* WAIT arrives -
    // an existing waiter can only ever be discovered by a later request, never retroactively,
    // matching Server/GameServer.cpp's handleQuickMatch shape exactly.
    void onWaitRequest(const ConnectionId& id, const std::string& username, int rating);

    // Background 1s-interval sweep for expired waiters - one shared thread, not one thread
    // per waiter (unlike WsGateway::redirectAndJoin's per-redirect timeout thread, which is
    // fine for rare, self-terminating redirects but would be an unbounded-growth risk here
    // given potentially many concurrent, up-to-60s waiters).
    void sweepLoop();

    int shardCount_;
    int quickMatchTimeoutMs_;
    std::shared_ptr<RedisWaitQueue> queue_;
    net::Logger& logger_;
    net::WsServerTransport downstream_;
    std::atomic<int> nextShardIndex_{0};
    std::atomic<bool> stopSweep_{false};

    // Guards waitingListMembers_ only - never call downstream_.send() while holding this: a
    // send to an already-disconnected peer can make IXWebSocket synchronously invoke
    // handleDisconnect on the calling thread before send() returns (see
    // Network/WsServerTransport.cpp's documented reentrancy hazard), which would then try to
    // re-lock this same non-recursive mutex - the same discipline Gateway/WsGateway.cpp
    // already follows everywhere for its own mutex_.
    std::mutex mutex_;
    std::unordered_map<ConnectionId, std::string> waitingListMembers_;

    std::thread sweepThread_;
};

}  // namespace kungfu
