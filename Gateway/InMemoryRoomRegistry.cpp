#include "InMemoryRoomRegistry.hpp"

namespace kungfu {

std::optional<std::string> InMemoryRoomRegistry::lookup(const std::string& roomKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = rooms_.find(roomKey);
    if (it == rooms_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void InMemoryRoomRegistry::registerRoom(const std::string& roomKey, const std::string& shardUrl) {
    std::lock_guard<std::mutex> lock(mutex_);
    rooms_[roomKey] = shardUrl;
}

}  // namespace kungfu
