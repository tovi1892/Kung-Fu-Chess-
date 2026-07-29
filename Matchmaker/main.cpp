#include <cstdlib>
#include <exception>
#include <memory>
#include <string>

#include "Matchmaker.hpp"
#include "RedisWaitQueue.hpp"

#include "Network/Logger.hpp"

using namespace kungfu;
using namespace kungfu::net;

namespace {

// Small, local to this binary rather than shared - same "three similar lines beats a shared
// abstraction for two tiny functions" call Gateway/main.cpp already makes.

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value != nullptr && *value != '\0') ? std::string(value) : fallback;
}

int envIntOr(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return fallback;
    }
}

}  // namespace

int main() {
    const int listenPort = envIntOr("MATCHMAKER_PORT", 9100);
    const int shardCount = envIntOr("SHARD_COUNT", 1);
    const int quickMatchTimeoutMs = envIntOr("MATCHMAKER_TIMEOUT_MS", 60000);

    Logger logger("MATCHMAKER", "logs/matchmaker.log");
    logger.log("starting on port " + std::to_string(listenPort) + ", shardCount=" + std::to_string(shardCount));

    auto queue =
        std::make_shared<RedisWaitQueue>(envOr("REDIS_HOST", "127.0.0.1"), envIntOr("REDIS_PORT", 6379));

    Matchmaker matchmaker(listenPort, shardCount, quickMatchTimeoutMs, queue, logger);
    matchmaker.run();
}
