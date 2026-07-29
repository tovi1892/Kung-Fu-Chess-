#pragma once

#include <string>

namespace kungfu {

struct LoginResult {
    bool success = false;
    std::string failureReason;  // meaningful only if !success, e.g. "bad_password"
    int rating = 0;             // meaningful only if success
    bool accountCreated = false;  // true only if this call auto-registered a brand-new
                                   // username, rather than validating an existing one
};

// Both accounts' ratings after an Elo update - see IAccountRepository::recordResult.
struct EloUpdateResult {
    int newWinnerRating = 0;
    int newLoserRating = 0;
};

// One concluded match, ready to persist - built by MatchResult.cpp from a Room + outcome.
// winnerUsername must equal whiteUsername or blackUsername (this variant has no draws).
// pgnData is a best-effort move log (see MatchResult.cpp's buildMoveLog), not a strict PGN
// export - may be empty. Kept free of PlayerColor/Logic types so this interface stays
// self-contained (color is implicit in which of whiteUsername/blackUsername matches).
struct MatchRecord {
    std::string whiteUsername;
    std::string blackUsername;
    std::string winnerUsername;
    std::string pgnData;
};

// Abstraction boundary between account storage/auth and the server logic that uses it
// (GameServer, MatchResult) - callers depend on this interface only, never on a concrete
// backing store. Swapping SQLite for something else means writing a new implementation of
// this interface and changing only the injected concrete class in main.cpp.
class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    // Auto-registers a brand-new username (starting rating 1200) with the given password;
    // an existing username must match its stored password or this fails with
    // "bad_password". LoginResult::accountCreated tells the caller which case happened,
    // rather than the caller having to guess.
    virtual LoginResult login(const std::string& username, const std::string& password) = 0;

    // Standard Elo update (K=32) for both accounts, plus persisting a `matches` row for the
    // match itself. Assumes both usernames already exist (only ever called for accounts that
    // just finished a match together, i.e. both already logged in successfully).
    virtual EloUpdateResult recordResult(const MatchRecord& match) = 0;
};

}  // namespace kungfu
