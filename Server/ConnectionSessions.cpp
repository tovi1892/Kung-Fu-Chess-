#include "ConnectionSessions.hpp"

namespace kungfu {

void ConnectionSessions::markPending(const std::string& id) {
    pending_.insert(id);
}

bool ConnectionSessions::isPending(const std::string& id) const {
    return pending_.count(id) > 0;
}

void ConnectionSessions::authenticate(const std::string& id, const std::string& username, int rating) {
    pending_.erase(id);
    authenticated_[id] = AuthenticatedSession{username, rating};
}

std::optional<AuthenticatedSession> ConnectionSessions::find(const std::string& id) const {
    const auto it = authenticated_.find(id);
    if (it == authenticated_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void ConnectionSessions::forget(const std::string& id) {
    pending_.erase(id);
    authenticated_.erase(id);
}

}  // namespace kungfu
