#include <cstdlib>
#include <exception>
#include <string>

#include "WsGateway.hpp"

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

}  // namespace

int main() {
    const int listenPort = envIntOr("GATEWAY_PORT", 9000);
    const std::string upstreamUrl = envOr("SHARD_URL", "ws://127.0.0.1:7777");

    Logger logger("GATEWAY", "logs/gateway.log");
    logger.log("starting on port " + std::to_string(listenPort) + ", forwarding to " + upstreamUrl);

    WsGateway gateway(listenPort, upstreamUrl);
    gateway.run();
}
