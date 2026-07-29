#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct redisContext;

namespace kungfu {

// One connection's QuickMatch search, as stored in Redis.
struct QueuedWaiter {
    std::string connectionId;
    std::string username;
    int rating;
    long long deadlineEpochMs;
};

// Redis-backed cross-shard QuickMatch waiting queue - pure Redis mechanics, no networking.
// Mirrors Server/RoomManager.hpp's quickMatchWaiting_ semantics exactly (matchWaiter's
// first-compatible-arrival rule, reapExpiredWaiters), but globally shared across every shard
// instead of one process's in-memory vector - see Matchmaker.hpp for why that matters.
//
// Backed by a Redis LIST (matchmaker:queue), not a sorted set: a ZSET ordered by rating would
// silently change the matching rule from "first compatible arrival" to "closest rating",
// which is not what RoomManager::matchWaiter does today. Same hiredis integration pattern as
// Gateway/RedisRoomRegistry.cpp (raw redisContext*, mutex-guarded, %b for binary-safe args).
// A single redisContext isn't safe to share across threads without external synchronization -
// same caveat RedisRoomRegistry.hpp/PostgresAccountRepository.hpp document for their own
// single-connection types.
class RedisWaitQueue {
public:
    // Throws std::runtime_error if the connection fails.
    RedisWaitQueue(const std::string& host, int port);
    ~RedisWaitQueue();

    RedisWaitQueue(const RedisWaitQueue&) = delete;
    RedisWaitQueue& operator=(const RedisWaitQueue&) = delete;

    // RPUSHes onto the queue - returns the exact serialized member string stored, so the
    // caller can hang onto it for a precise later removeWaiterByMember (disconnect cleanup).
    std::string addWaiter(const QueuedWaiter& waiter);

    // Scans the queue oldest-to-newest for the first entry with
    // abs(rating - candidateRating) <= eloRange; if found, removes and returns it.
    std::optional<QueuedWaiter> matchWaiter(int rating, int eloRange);

    // Removes one exact previously-returned member string - a no-op if it's already gone
    // (e.g. reaped or matched by a concurrent request just before this call).
    void removeWaiterByMember(const std::string& member);

    // Removes and returns every entry whose deadline has passed as of nowEpochMs - the
    // caller still owns notifying each one and any further bookkeeping.
    std::vector<QueuedWaiter> reapExpired(long long nowEpochMs);

    // Globally unique across every shard (unlike RoomManager::allocateQuickMatchRoomKey's
    // per-process counter) - "quickmatch-" + a Redis INCR value.
    std::string allocateRoomKey();

private:
    redisContext* ctx_;
    std::mutex mutex_;
};

}  // namespace kungfu
