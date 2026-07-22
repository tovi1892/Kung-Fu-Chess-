#pragma once

#include <string>

struct sqlite3;

namespace kungfu {

struct LoginResult {
    bool success = false;
    std::string failureReason;  // meaningful only if !success, e.g. "bad_password"
    int rating = 0;             // meaningful only if success
    bool accountCreated = false;  // true only if this call auto-registered a brand-new
                                   // username, rather than validating an existing one
};

// Both accounts' ratings after an Elo update - see AccountStore::recordResult.
struct EloUpdateResult {
    int newWinnerRating = 0;
    int newLoserRating = 0;
};

// SQLite-backed account storage - the only place allowed to touch sqlite3_* directly,
// same "one wrapper per external library" convention as Img/SoundPlayer/WsServerTransport.
// No internal mutex: relies on SQLite's own default serialized threading mode
// (SQLITE_THREADSAFE=1, set explicitly on the vendored amalgamation in CMakeLists.txt),
// which already makes concurrent calls on the same connection handle safe.
class AccountStore {
public:
    // Opens (creating if needed) the database at dbPath and ensures the accounts table
    // exists.
    explicit AccountStore(const std::string& dbPath);
    ~AccountStore();

    AccountStore(const AccountStore&) = delete;
    AccountStore& operator=(const AccountStore&) = delete;

    // Auto-registers a brand-new username (starting rating 1200) with the given password;
    // an existing username must match its stored password or this fails with
    // "bad_password". LoginResult::accountCreated tells the caller which case happened,
    // rather than the caller having to guess. Never stores the password itself - see the
    // .cpp for the PBKDF2/CNG hashing this wraps.
    LoginResult login(const std::string& username, const std::string& password);

    // Standard Elo update (K=32) for both accounts - see the .cpp for the exact formula.
    // Assumes both usernames already exist (only ever called for accounts that just
    // finished a match together, i.e. both already logged in successfully).
    EloUpdateResult recordResult(const std::string& winnerUsername, const std::string& loserUsername);

private:
    sqlite3* db_ = nullptr;
};

}  // namespace kungfu
