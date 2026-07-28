#pragma once

#include <string>

namespace kungfu {

// Deployment configuration read from environment variables, falling back to these defaults
// when unset - lets the same kungfu_server binary run either as a plain local process
// (every default applies, matching this project's existing native dev setup) or as a
// container wired up by docker-compose.yml/k8s/configmap.yaml (every value overridden).
//
// port's fallback stays 7777 (not the 8080 docker-compose.yml/k8s manifests use) so a local
// run with no environment configured behaves exactly as it always has - every container
// manifest sets PORT explicitly rather than relying on this fallback, so this doesn't affect
// them either way.
struct EnvConfig {
    std::string dbHost = "localhost";
    int dbPort = 5432;
    std::string dbUser = "kungfu_admin";
    std::string dbPassword = "secretpassword";
    std::string dbName = "kungfu_chess";
    int port = 7777;
};

EnvConfig loadEnvConfig();

}  // namespace kungfu
