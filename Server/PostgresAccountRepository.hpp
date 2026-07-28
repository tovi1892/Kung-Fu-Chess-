#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "IAccountRepository.hpp"

// A forward declaration doesn't work here - libpqxx 6.4 (Ubuntu 22.04's libpqxx-dev, and the
// version this actually has to compile against) declares pqxx::connection as a type alias
// (`using connection = basic_connection_base<connect_direct>;`), not a class, so `class
// connection;` conflicts with it outright and corrupts every subsequent pqxx header's parse.
// The full header is only ever needed by this file and PostgresAccountRepository.cpp, both
// already gated behind KUNGFU_ENABLE_POSTGRES, so including it here costs nothing extra.
#include <pqxx/pqxx>

namespace kungfu {

// PostgreSQL-backed account storage - implements IAccountRepository identically to
// SqliteAccountRepository (same PBKDF2-HMAC-SHA256 scheme via OpenSSL rather than Windows
// CNG, same login()/recordResult() contract and behavior) but against a networked Postgres
// server instead of an embedded file, per Server_Design.md's scaling rationale. Compiled
// only when KUNGFU_ENABLE_POSTGRES is on (see CMakeLists.txt) - the native Windows/SQLite
// dev build never touches this file or needs libpqxx/OpenSSL installed at all.
//
// Unlike SqliteAccountRepository (which relies on SQLite's own SQLITE_THREADSAFE serialized
// mode), a single pqxx::connection is not safe to share across threads without external
// synchronization - guarded here by a private mutex, since GameServer can call
// login()/recordResult() from multiple rooms' independently-locked threads concurrently
// (see Room.hpp's per-room locking).
class PostgresAccountRepository : public IAccountRepository {
public:
    // Connects using a libpq keyword/value connection string built from host/port/user/
    // password/dbName (see EnvConfig for where these come from - main() passes them
    // straight through). Throws std::runtime_error if the connection fails; assumes
    // init-db.sql's `users` table already exists (docker-compose.yml runs it via Postgres's
    // docker-entrypoint-initdb.d on first container start).
    PostgresAccountRepository(const std::string& host, int port, const std::string& user,
                               const std::string& password, const std::string& dbName);
    ~PostgresAccountRepository() override;

    PostgresAccountRepository(const PostgresAccountRepository&) = delete;
    PostgresAccountRepository& operator=(const PostgresAccountRepository&) = delete;

    LoginResult login(const std::string& username, const std::string& password) override;
    EloUpdateResult recordResult(const std::string& winnerUsername, const std::string& loserUsername) override;

private:
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex mutex_;
};

}  // namespace kungfu
