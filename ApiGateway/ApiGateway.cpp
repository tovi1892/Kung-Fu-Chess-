#include "ApiGateway.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include <ixwebsocket/IXNetSystem.h>

namespace kungfu {

namespace {

constexpr int kDefaultLeaderboardLimit = 20;
constexpr int kMaxLeaderboardLimit = 100;

// Splits on delimiter, dropping empty tokens - same "split, skip empties" idiom
// Gateway/main.cpp's splitCsv already uses, applied here to both '/' path segments and '&'
// query pairs. Duplicated rather than shared across files, matching this project's own
// "three similar lines beats a shared abstraction for two tiny functions" precedent.
std::vector<std::string> splitSkipEmpty(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t pos = text.find(delimiter, start);
        const std::size_t end = pos == std::string::npos ? text.size() : pos;
        if (end > start) {
            parts.push_back(text.substr(start, end - start));
        }
        if (pos == std::string::npos) {
            break;
        }
        start = pos + 1;
    }
    return parts;
}

bool isValidUsername(const std::string& username) {
    if (username.size() < 3 || username.size() > 32) {
        return false;
    }
    return std::all_of(username.begin(), username.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

int parseLimit(const std::string& queryString) {
    for (const auto& pair : splitSkipEmpty(queryString, '&')) {
        const auto eq = pair.find('=');
        if (eq == std::string::npos || pair.substr(0, eq) != "limit") {
            continue;
        }
        try {
            return std::clamp(std::stoi(pair.substr(eq + 1)), 1, kMaxLeaderboardLimit);
        } catch (const std::exception&) {
            break;
        }
    }
    return kDefaultLeaderboardLimit;
}

nlohmann::json profileToJson(const UserProfile& profile) {
    return nlohmann::json{{"username", profile.username}, {"rating", profile.rating},
                           {"wins", profile.wins}, {"losses", profile.losses}};
}

}  // namespace

ApiGateway::ApiGateway(int listenPort, std::shared_ptr<PostgresAccountRepository> accounts, net::Logger& logger)
    : listenPort_(listenPort), accounts_(std::move(accounts)), logger_(logger) {
    ix::initNetSystem();
    httpServer_ = std::make_unique<ix::HttpServer>(listenPort_, "0.0.0.0");
}

void ApiGateway::run() {
    httpServer_->setOnConnectionCallback(
        [this](const ix::HttpRequestPtr& request, const std::shared_ptr<ix::ConnectionState>&) {
            return handleRequest(request);
        });

    if (!httpServer_->listenAndStart()) {
        throw std::runtime_error("ApiGateway: failed to listen on port " + std::to_string(listenPort_));
    }
    httpServer_->wait();  // blocks until stop() is called, which nothing here does

    // wait()'s signature doesn't let the compiler statically verify [[noreturn]] - this
    // mirrors WsGateway::run()'s explicit trailing loop for the same reason.
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(3600));
    }
}

ix::HttpResponsePtr ApiGateway::jsonError(int statusCode, const std::string& description,
                                           const std::string& errorCode) {
    return jsonOk(statusCode, description, nlohmann::json{{"error", errorCode}});
}

ix::HttpResponsePtr ApiGateway::jsonOk(int statusCode, const std::string& description, const nlohmann::json& body) {
    ix::WebSocketHttpHeaders headers;
    headers["Content-Type"] = "application/json";
    return std::make_shared<ix::HttpResponse>(statusCode, description, ix::HttpErrorCode::Ok, headers, body.dump());
}

ix::HttpResponsePtr ApiGateway::handleRequest(const ix::HttpRequestPtr& request) {
    const std::string& uri = request->uri;
    const auto queryPos = uri.find('?');
    const std::string path = uri.substr(0, queryPos);
    const std::string queryString = queryPos == std::string::npos ? "" : uri.substr(queryPos + 1);
    const std::vector<std::string> segments = splitSkipEmpty(path, '/');

    if (request->method == "POST" && path == "/register") {
        return handleRegister(request->body);
    }
    if (request->method == "POST" && path == "/login") {
        return handleLogin(request->body);
    }
    if (request->method == "GET" && segments.size() == 2 && segments[0] == "users") {
        return handleGetUser(segments[1]);
    }
    if (request->method == "GET" && path == "/leaderboard") {
        return handleLeaderboard(queryString);
    }
    return jsonError(404, "Not Found", "not_found");
}

ix::HttpResponsePtr ApiGateway::handleRegister(const std::string& body) {
    const auto json = nlohmann::json::parse(body, nullptr, false);
    if (json.is_discarded() || !json.contains("username") || !json.contains("password") ||
        !json["username"].is_string() || !json["password"].is_string()) {
        return jsonError(400, "Bad Request", "invalid_request");
    }
    const std::string username = json["username"].get<std::string>();
    const std::string password = json["password"].get<std::string>();
    if (!isValidUsername(username) || password.empty()) {
        return jsonError(400, "Bad Request", "invalid_request");
    }

    const RegisterResult result = accounts_->registerAccount(username, password);
    if (!result.success) {
        return jsonError(409, "Conflict", result.failureReason);
    }
    return jsonOk(201, "Created", nlohmann::json{{"username", username}, {"rating", result.rating}});
}

ix::HttpResponsePtr ApiGateway::handleLogin(const std::string& body) {
    const auto json = nlohmann::json::parse(body, nullptr, false);
    if (json.is_discarded() || !json.contains("username") || !json.contains("password") ||
        !json["username"].is_string() || !json["password"].is_string()) {
        return jsonError(400, "Bad Request", "invalid_request");
    }
    const std::string username = json["username"].get<std::string>();
    const std::string password = json["password"].get<std::string>();

    const StrictLoginResult result = accounts_->loginStrict(username, password);
    if (!result.success) {
        const int statusCode = result.failureReason == "not_found" ? 404 : 401;
        return jsonError(statusCode, statusCode == 404 ? "Not Found" : "Unauthorized", result.failureReason);
    }
    return jsonOk(200, "OK",
                   nlohmann::json{{"username", username},
                                  {"rating", result.rating},
                                  {"wins", result.wins},
                                  {"losses", result.losses}});
}

ix::HttpResponsePtr ApiGateway::handleGetUser(const std::string& username) {
    const auto profile = accounts_->getProfile(username);
    if (!profile.has_value()) {
        return jsonError(404, "Not Found", "not_found");
    }
    return jsonOk(200, "OK", profileToJson(*profile));
}

ix::HttpResponsePtr ApiGateway::handleLeaderboard(const std::string& queryString) {
    const int limit = parseLimit(queryString);
    const auto leaderboard = accounts_->getLeaderboard(limit);

    nlohmann::json array = nlohmann::json::array();
    for (const auto& profile : leaderboard) {
        array.push_back(profileToJson(profile));
    }
    return jsonOk(200, "OK", array);
}

}  // namespace kungfu
