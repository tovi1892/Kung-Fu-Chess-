#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace kungfu {

// An authenticated (LOGIN_OK'd) connection that hasn't necessarily joined a room yet.
struct AuthenticatedSession {
    std::string username;
    int rating;
};

// Tracks each connection's identity-transition state: pending (opened, not yet logged in)
// -> authenticated (LOGIN succeeded). Doesn't know about IAccountRepository, sockets, or the
// wire protocol - Server/main.cpp still owns calling IAccountRepository::login() and sending
// LOGIN_OK/LOGIN_FAIL; this class only records the resulting state so onMessage/onDisconnect
// can look up "who is this" without re-deriving it.
//
// Not thread-safe on its own: every method assumes the caller already holds
// Server/main.cpp's gameMutex, same contract as RoomManager/Outbox.
class ConnectionSessions {
public:
    // Called from onConnect - a fresh connection is pending until LOGIN succeeds.
    void markPending(const std::string& id);
    bool isPending(const std::string& id) const;

    // Transitions `id` from pending to authenticated. Only meaningful (and only ever
    // called) right after a successful IAccountRepository::login() - main.cpp still owns deciding
    // success/failure and sending the reply either way.
    void authenticate(const std::string& id, const std::string& username, int rating);

    // The authenticated session for `id`, or nullopt if it's still pending or unknown.
    std::optional<AuthenticatedSession> find(const std::string& id) const;

    // Forgets this connection entirely - pending or authenticated, doesn't matter which -
    // called unconditionally on disconnect.
    void forget(const std::string& id);

private:
    std::unordered_set<std::string> pending_;
    std::unordered_map<std::string, AuthenticatedSession> authenticated_;
};

}  // namespace kungfu
