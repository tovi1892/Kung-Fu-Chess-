#include "RedisRoomRegistry.hpp"

#include <hiredis/hiredis.h>

#include <stdexcept>

namespace kungfu {

namespace {
constexpr char kKeyPrefix[] = "room:";
}  // namespace

RedisRoomRegistry::RedisRoomRegistry(const std::string& host, int port) {
    ctx_ = redisConnect(host.c_str(), port);
    if (ctx_ == nullptr || ctx_->err) {
        const std::string reason = ctx_ != nullptr ? ctx_->errstr : "redisConnect returned null";
        if (ctx_ != nullptr) {
            redisFree(ctx_);
        }
        throw std::runtime_error("Failed to connect to Redis at " + host + ": " + reason);
    }
}

RedisRoomRegistry::~RedisRoomRegistry() {
    redisFree(ctx_);
}

std::optional<std::string> RedisRoomRegistry::lookup(const std::string& roomKey) {
    std::lock_guard<std::mutex> lock(mutex_);

    // %b (explicit-length), not %s - roomKey is arbitrary client text and %s is strlen-based,
    // silently truncating at an embedded NUL. See IRoomRegistry.hpp/RedisRoomRegistry.hpp.
    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx_, "GET %b%b", kKeyPrefix, sizeof(kKeyPrefix) - 1, roomKey.data(), roomKey.size()));
    if (reply == nullptr) {
        return std::nullopt;
    }

    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    }
    freeReplyObject(reply);
    return result;
}

void RedisRoomRegistry::registerRoom(const std::string& roomKey, const std::string& shardUrl) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "SET %b%b %b", kKeyPrefix, sizeof(kKeyPrefix) - 1,
                                                          roomKey.data(), roomKey.size(), shardUrl.data(),
                                                          shardUrl.size()));
    if (reply != nullptr) {
        freeReplyObject(reply);
    }
}

}  // namespace kungfu
