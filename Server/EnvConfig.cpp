#include "EnvConfig.hpp"

#include <cstdlib>
#include <exception>

namespace kungfu {

namespace {

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
        return fallback;  // malformed env var - defensive, keep the default rather than crash
    }
}

}  // namespace

EnvConfig loadEnvConfig() {
    EnvConfig config;
    config.dbHost = envOr("DB_HOST", config.dbHost);
    config.dbPort = envIntOr("DB_PORT", config.dbPort);
    config.dbUser = envOr("DB_USER", config.dbUser);
    config.dbPassword = envOr("DB_PASSWORD", config.dbPassword);
    config.dbName = envOr("DB_NAME", config.dbName);
    config.port = envIntOr("PORT", config.port);
    return config;
}

}  // namespace kungfu
