#pragma once

#include <string>

#include "IAccountRepository.hpp"

struct sqlite3;

namespace kungfu {

// SQLite-backed account storage - the only place allowed to touch sqlite3_* directly, same
// "one wrapper per external library" convention as Img/SoundPlayer/WsServerTransport.
// No internal mutex: relies on SQLite's own default serialized threading mode
// (SQLITE_THREADSAFE=1, set explicitly on the vendored amalgamation in CMakeLists.txt),
// which already makes concurrent calls on the same connection handle safe.
class SqliteAccountRepository : public IAccountRepository {
public:
    // Opens (creating if needed) the database at dbPath and ensures the accounts table
    // exists.
    explicit SqliteAccountRepository(const std::string& dbPath);
    ~SqliteAccountRepository() override;

    SqliteAccountRepository(const SqliteAccountRepository&) = delete;
    SqliteAccountRepository& operator=(const SqliteAccountRepository&) = delete;

    LoginResult login(const std::string& username, const std::string& password) override;
    EloUpdateResult recordResult(const std::string& winnerUsername, const std::string& loserUsername) override;

private:
    sqlite3* db_ = nullptr;
};

}  // namespace kungfu
