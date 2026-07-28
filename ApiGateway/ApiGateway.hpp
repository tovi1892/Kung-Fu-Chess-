#pragma once

#include <memory>
#include <string>

#include <ixwebsocket/IXHttpServer.h>
#include <nlohmann/json.hpp>

#include "Server/PostgresAccountRepository.hpp"

#include "Network/Logger.hpp"

namespace kungfu {

// Stateless REST facade over PostgresAccountRepository - login/register/profile/leaderboard
// over plain HTTP, decoupled from the realtime WS path (see Server_Design.md's target
// architecture: "Auth & REST Microservices" as a separate box from the game engine). Does
// NOT implement IAccountRepository or touch kungfu_server's WS LOGIN flow in any way - this
// is a strictly additive HTTP surface; the existing WS LOGIN message flow into kungfu_server
// (Server/GameServer.cpp::handleLogin) is completely unchanged, and there is no session-token
// handoff between this service and the WS Gateway yet (a distinctly separate, larger,
// deliberately deferred feature).
//
// Unlike WsGateway, there is no per-connection routing state to hold at all - every HTTP
// request is independently handled against the same PostgresAccountRepository, which already
// owns its own mutex (see PostgresAccountRepository.hpp) - so this class needs no mutex of
// its own despite ix::HttpServer dispatching concurrent requests on their own per-connection
// threads (same model Network/WsServerTransport.cpp already wraps for WebSocket).
class ApiGateway {
public:
    // logger must outlive this object, same contract as WsGateway's constructor.
    ApiGateway(int listenPort, std::shared_ptr<PostgresAccountRepository> accounts, net::Logger& logger);

    // Starts the HTTP listener and blocks forever - the last call main() makes.
    [[noreturn]] void run();

private:
    ix::HttpResponsePtr handleRequest(const ix::HttpRequestPtr& request);

    ix::HttpResponsePtr handleRegister(const std::string& body);
    ix::HttpResponsePtr handleLogin(const std::string& body);
    ix::HttpResponsePtr handleGetUser(const std::string& username);
    ix::HttpResponsePtr handleLeaderboard(const std::string& queryString);

    static ix::HttpResponsePtr jsonError(int statusCode, const std::string& description, const std::string& errorCode);
    static ix::HttpResponsePtr jsonOk(int statusCode, const std::string& description, const nlohmann::json& body);

    int listenPort_;
    std::shared_ptr<PostgresAccountRepository> accounts_;
    net::Logger& logger_;

    // unique_ptr, not a direct member: ix::initNetSystem() (WSAStartup on Windows) must run
    // before any ix::SocketServer-derived object is constructed - see
    // Network/WsServerTransport.cpp's identical ordering. A direct member would be
    // constructed via this class's initializer list, before the constructor body's call to
    // initNetSystem() ever runs.
    std::unique_ptr<ix::HttpServer> httpServer_;
};

}  // namespace kungfu
