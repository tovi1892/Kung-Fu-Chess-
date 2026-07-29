#include <cctype>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "WsGateway.hpp"

#ifdef KUNGFU_ENABLE_REDIS
#include "RedisRoomRegistry.hpp"
#else
#include "InMemoryRoomRegistry.hpp"
#endif

#include "Network/Logger.hpp"

using namespace kungfu;
using namespace kungfu::net;

namespace {

// Small, local to this binary rather than reused from Server/EnvConfig.cpp - that struct is
// specifically about the Shard's DB/listen-port configuration, and this Gateway has nothing
// to do with a database at all. Same "three similar lines beats a shared abstraction for two
// tiny functions" call as elsewhere in this project.

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

// "ws://a:1,ws://b:2" -> ["ws://a:1", "ws://b:2"] - trims surrounding whitespace per entry so
// a docker-compose-style multi-line env value with accidental spaces still parses cleanly.
std::vector<std::string> splitCsv(const std::string& csv) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= csv.size()) {
        const std::size_t comma = csv.find(',', start);
        const std::size_t end = comma == std::string::npos ? csv.size() : comma;
        std::size_t first = start;
        while (first < end && std::isspace(static_cast<unsigned char>(csv[first]))) {
            ++first;
        }
        std::size_t last = end;
        while (last > first && std::isspace(static_cast<unsigned char>(csv[last - 1]))) {
            --last;
        }
        if (last > first) {
            parts.push_back(csv.substr(first, last - first));
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return parts;
}

}  // namespace

int main() {
    const int listenPort = envIntOr("GATEWAY_PORT", 9000);
    const std::string shardUrlsRaw = envOr("SHARD_URLS", "ws://127.0.0.1:7777");
    const std::vector<std::string> shardUrls = splitCsv(shardUrlsRaw);
    const std::string matchmakerUrl = envOr("MATCHMAKER_URL", "ws://127.0.0.1:9100");

    Logger logger("GATEWAY", "logs/gateway.log");
    logger.log("starting on port " + std::to_string(listenPort) + ", shards: " + shardUrlsRaw +
               ", matchmaker: " + matchmakerUrl);

#ifdef KUNGFU_ENABLE_REDIS
    auto roomRegistry =
        std::make_shared<RedisRoomRegistry>(envOr("REDIS_HOST", "127.0.0.1"), envIntOr("REDIS_PORT", 6379));
#else
    auto roomRegistry = std::make_shared<InMemoryRoomRegistry>();
#endif

    WsGateway gateway(listenPort, shardUrls, matchmakerUrl, roomRegistry, logger);
    gateway.run();
}
