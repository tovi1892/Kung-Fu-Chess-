#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "IRoomRegistry.hpp"

namespace kungfu {

// Default IRoomRegistry backend (KUNGFU_ENABLE_REDIS off) - correct only within one Gateway
// process. Fine today since docker-compose.yml/k8s only ever run a single `gateway` service;
// this stops being sufficient the moment there's more than one Gateway replica sharing
// routing state, at which point RedisRoomRegistry is what's needed, not a fix to this class.
class InMemoryRoomRegistry : public IRoomRegistry {
public:
    std::optional<std::string> lookup(const std::string& roomKey) override;
    void registerRoom(const std::string& roomKey, const std::string& shardUrl) override;

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::string> rooms_;
};

}  // namespace kungfu
