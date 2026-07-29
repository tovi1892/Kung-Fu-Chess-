#include "RedisWaitQueue.hpp"

#include <hiredis/hiredis.h>

#include <cmath>
#include <stdexcept>

namespace kungfu {

namespace {

constexpr char kQueueKey[] = "matchmaker:queue";
constexpr char kCounterKey[] = "matchmaker:roomkey:counter";

// Pipe-delimited - usernames are already assumed space/pipe-free by the existing
// "LOGIN <username> <password>" wire format (Network/Protocol.cpp), so no new escaping
// burden is introduced here.
std::string serialize(const QueuedWaiter& w) {
    return w.connectionId + "|" + w.username + "|" + std::to_string(w.rating) + "|" +
           std::to_string(w.deadlineEpochMs);
}

std::optional<QueuedWaiter> deserialize(const std::string& member) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = member.find('|', start);
        parts.push_back(member.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos) {
            break;
        }
        start = pos + 1;
    }
    if (parts.size() != 4) {
        return std::nullopt;
    }
    try {
        return QueuedWaiter{parts[0], parts[1], std::stoi(parts[2]), std::stoll(parts[3])};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// Fetches and deserializes every current queue member - callers scan/filter client-side,
// same O(n) shape RoomManager::matchWaiter/reapExpiredWaiters already have in-memory.
std::vector<std::string> lrangeAll(redisContext* ctx) {
    std::vector<std::string> members;
    auto* reply = static_cast<redisReply*>(redisCommand(ctx, "LRANGE %s 0 -1", kQueueKey));
    if (reply == nullptr) {
        return members;
    }
    if (reply->type == REDIS_REPLY_ARRAY) {
        members.reserve(reply->elements);
        for (std::size_t i = 0; i < reply->elements; ++i) {
            members.emplace_back(reply->element[i]->str, reply->element[i]->len);
        }
    }
    freeReplyObject(reply);
    return members;
}

void lremOne(redisContext* ctx, const std::string& member) {
    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx, "LREM %s 1 %b", kQueueKey, member.data(), member.size()));
    if (reply != nullptr) {
        freeReplyObject(reply);
    }
}

}  // namespace

RedisWaitQueue::RedisWaitQueue(const std::string& host, int port) {
    ctx_ = redisConnect(host.c_str(), port);
    if (ctx_ == nullptr || ctx_->err) {
        const std::string reason = ctx_ != nullptr ? ctx_->errstr : "redisConnect returned null";
        if (ctx_ != nullptr) {
            redisFree(ctx_);
        }
        throw std::runtime_error("Failed to connect to Redis at " + host + ": " + reason);
    }
}

RedisWaitQueue::~RedisWaitQueue() {
    redisFree(ctx_);
}

std::string RedisWaitQueue::addWaiter(const QueuedWaiter& waiter) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string member = serialize(waiter);
    auto* reply =
        static_cast<redisReply*>(redisCommand(ctx_, "RPUSH %s %b", kQueueKey, member.data(), member.size()));
    if (reply != nullptr) {
        freeReplyObject(reply);
    }
    return member;
}

std::optional<QueuedWaiter> RedisWaitQueue::matchWaiter(int rating, int eloRange) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& member : lrangeAll(ctx_)) {
        const auto waiter = deserialize(member);
        if (waiter.has_value() && std::abs(waiter->rating - rating) <= eloRange) {
            lremOne(ctx_, member);
            return waiter;
        }
    }
    return std::nullopt;
}

void RedisWaitQueue::removeWaiterByMember(const std::string& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    lremOne(ctx_, member);
}

std::vector<QueuedWaiter> RedisWaitQueue::reapExpired(long long nowEpochMs) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<QueuedWaiter> expired;
    for (const auto& member : lrangeAll(ctx_)) {
        const auto waiter = deserialize(member);
        if (waiter.has_value() && waiter->deadlineEpochMs <= nowEpochMs) {
            lremOne(ctx_, member);
            expired.push_back(*waiter);
        }
    }
    return expired;
}

std::string RedisWaitQueue::allocateRoomKey() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "INCR %s", kCounterKey));
    long long value = 0;
    if (reply != nullptr) {
        if (reply->type == REDIS_REPLY_INTEGER) {
            value = reply->integer;
        }
        freeReplyObject(reply);
    }
    return "quickmatch-" + std::to_string(value);
}

}  // namespace kungfu
