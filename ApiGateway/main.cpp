#include <cstdlib>
#include <exception>
#include <memory>
#include <string>

#include "ApiGateway.hpp"

#include "Server/EnvConfig.hpp"
#include "Server/PostgresAccountRepository.hpp"

#include "Network/Logger.hpp"

using namespace kungfu;
using namespace kungfu::net;

namespace {

// API_GATEWAY_PORT is kept independent of EnvConfig::port (which drives each shard's PORT)
// so docker-compose can set them independently, matching every other service's one-env-var-
// per-process convention (GATEWAY_PORT for the WS gateway, PORT for each shard). Same small
// local helper Gateway/main.cpp already has - not worth sharing for one function.
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
    const EnvConfig config = loadEnvConfig();
    const int listenPort = envIntOr("API_GATEWAY_PORT", 8081);

    Logger logger("API_GATEWAY", "logs/api_gateway.log");
    logger.log("starting on port " + std::to_string(listenPort));

    auto accounts = std::make_shared<PostgresAccountRepository>(config.dbHost, config.dbPort, config.dbUser,
                                                                  config.dbPassword, config.dbName);

    ApiGateway apiGateway(listenPort, accounts, logger);
    apiGateway.run();
}
