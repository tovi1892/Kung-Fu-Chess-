#include "PostgresAccountRepository.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <pqxx/pqxx>

namespace kungfu {

namespace {

constexpr int kSaltLength = 16;
constexpr int kHashLength = 32;  // SHA-256 output size
constexpr int kPbkdf2Iterations = 100000;
constexpr int kStartingRating = 1200;
constexpr int kEloKFactor = 32;

// --- Password hashing (OpenSSL PKCS5_PBKDF2_HMAC) --------------------------------------
// Same algorithm/parameters as SqliteAccountRepository's Windows-CNG version (PBKDF2-
// HMAC-SHA256, 100000 iterations, 32-byte output) so a given password always derives the
// same hash regardless of which IAccountRepository backend is running - just a portable
// (Linux-buildable) equivalent, since bcrypt.h/BCryptDeriveKeyPBKDF2 are Windows-only and
// this file is compiled inside Dockerfile.server's Ubuntu container.

std::vector<unsigned char> randomBytes(int count) {
    std::vector<unsigned char> buffer(count);
    if (RAND_bytes(buffer.data(), count) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return buffer;
}

std::vector<unsigned char> derivePbkdf2(const std::string& password, const std::vector<unsigned char>& salt) {
    std::vector<unsigned char> derived(kHashLength);
    const int ok = PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.data(),
                                      static_cast<int>(salt.size()), kPbkdf2Iterations, EVP_sha256(), kHashLength,
                                      derived.data());
    if (ok != 1) {
        throw std::runtime_error("PKCS5_PBKDF2_HMAC failed");
    }
    return derived;
}

// hash/salt round-trip through init-db.sql's TEXT columns as hex, not BYTEA - see
// init-db.sql's comment for why (only libpqxx's plain std::string param/result path, the
// most stable part of its API across versions, could be exercised without compile-testing
// against a real libpqxx locally).

std::string toHex(const std::vector<unsigned char>& bytes) {
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        out.push_back(kDigits[b >> 4]);
        out.push_back(kDigits[b & 0x0F]);
    }
    return out;
}

std::vector<unsigned char> fromHex(const std::string& hex) {
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<unsigned char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

}  // namespace

PostgresAccountRepository::PostgresAccountRepository(const std::string& host, int port, const std::string& user,
                                                       const std::string& password, const std::string& dbName) {
    std::ostringstream connStr;
    connStr << "host=" << host << " port=" << port << " user=" << user << " password=" << password
            << " dbname=" << dbName;
    conn_ = std::make_unique<pqxx::connection>(connStr.str());
    if (!conn_->is_open()) {
        throw std::runtime_error("Failed to open PostgreSQL connection to " + host);
    }
}

PostgresAccountRepository::~PostgresAccountRepository() = default;

LoginResult PostgresAccountRepository::login(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    pqxx::work txn(*conn_);
    const auto result = txn.exec_params(
        "SELECT password_hash, password_salt, elo_rating FROM users WHERE username = $1;", username);

    if (!result.empty()) {
        const auto row = result[0];
        const auto storedHash = fromHex(row[0].as<std::string>());
        const auto salt = fromHex(row[1].as<std::string>());
        const int rating = row[2].as<int>();
        txn.commit();

        if (derivePbkdf2(password, salt) == storedHash) {
            return LoginResult{true, "", rating, false};
        }
        return LoginResult{false, "bad_password", 0, false};
    }

    // Unknown username - auto-register a fresh account rather than requiring a separate
    // sign-up step, same policy as SqliteAccountRepository::login.
    const auto salt = randomBytes(kSaltLength);
    const auto hash = derivePbkdf2(password, salt);
    txn.exec_params(
        "INSERT INTO users (username, password_hash, password_salt, elo_rating) VALUES ($1, $2, $3, $4);", username,
        toHex(hash), toHex(salt), kStartingRating);
    txn.commit();

    return LoginResult{true, "", kStartingRating, true};
}

EloUpdateResult PostgresAccountRepository::recordResult(const std::string& winnerUsername,
                                                          const std::string& loserUsername) {
    std::lock_guard<std::mutex> lock(mutex_);

    pqxx::work txn(*conn_);
    const int winnerRating =
        txn.exec_params("SELECT elo_rating FROM users WHERE username = $1;", winnerUsername)[0][0].as<int>();
    const int loserRating =
        txn.exec_params("SELECT elo_rating FROM users WHERE username = $1;", loserUsername)[0][0].as<int>();

    // Same Elo formula as SqliteAccountRepository::recordResult (K=32, expected score from
    // the rating gap, loser's delta derived from the winner's actual delta so total rating
    // movement is exactly conserved) - see that file's comment for the full reasoning.
    const double expectedWinner = 1.0 / (1.0 + std::pow(10.0, (loserRating - winnerRating) / 400.0));
    const int newWinnerRating = static_cast<int>(std::lround(winnerRating + kEloKFactor * (1.0 - expectedWinner)));
    const int newLoserRating = loserRating - (newWinnerRating - winnerRating);

    txn.exec_params("UPDATE users SET elo_rating = $1, wins = wins + 1 WHERE username = $2;", newWinnerRating,
                     winnerUsername);
    txn.exec_params("UPDATE users SET elo_rating = $1, losses = losses + 1 WHERE username = $2;", newLoserRating,
                     loserUsername);
    txn.commit();

    return EloUpdateResult{newWinnerRating, newLoserRating};
}

RegisterResult PostgresAccountRepository::registerAccount(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    pqxx::work txn(*conn_);
    const auto existing = txn.exec_params("SELECT 1 FROM users WHERE username = $1;", username);
    if (!existing.empty()) {
        return RegisterResult{false, "username_taken", 0};
    }

    const auto salt = randomBytes(kSaltLength);
    const auto hash = derivePbkdf2(password, salt);
    try {
        txn.exec_params(
            "INSERT INTO users (username, password_hash, password_salt, elo_rating) VALUES ($1, $2, $3, $4);",
            username, toHex(hash), toHex(salt), kStartingRating);
        txn.commit();
    } catch (const pqxx::unique_violation&) {
        // Defense-in-depth against a concurrent registration of the same username landing
        // between the SELECT above and this INSERT - the per-instance mutex_ already
        // serializes every call into this one object today (so this should be unreachable in
        // practice), but the DB's own UNIQUE constraint is the theoretically-correct
        // backstop, not something to rely on the mutex alone for long-term.
        return RegisterResult{false, "username_taken", 0};
    }

    return RegisterResult{true, "", kStartingRating};
}

StrictLoginResult PostgresAccountRepository::loginStrict(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    pqxx::work txn(*conn_);
    const auto result = txn.exec_params(
        "SELECT password_hash, password_salt, elo_rating, wins, losses FROM users WHERE username = $1;", username);
    txn.commit();

    if (result.empty()) {
        return StrictLoginResult{false, "not_found", 0, 0, 0};
    }

    const auto row = result[0];
    const auto storedHash = fromHex(row[0].as<std::string>());
    const auto salt = fromHex(row[1].as<std::string>());
    if (derivePbkdf2(password, salt) != storedHash) {
        return StrictLoginResult{false, "bad_password", 0, 0, 0};
    }

    return StrictLoginResult{true, "", row[2].as<int>(), row[3].as<int>(), row[4].as<int>()};
}

std::optional<UserProfile> PostgresAccountRepository::getProfile(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    pqxx::work txn(*conn_);
    const auto result =
        txn.exec_params("SELECT elo_rating, wins, losses FROM users WHERE username = $1;", username);
    txn.commit();

    if (result.empty()) {
        return std::nullopt;
    }
    const auto row = result[0];
    return UserProfile{username, row[0].as<int>(), row[1].as<int>(), row[2].as<int>()};
}

std::vector<UserProfile> PostgresAccountRepository::getLeaderboard(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);

    pqxx::work txn(*conn_);
    const auto result = txn.exec_params(
        "SELECT username, elo_rating, wins, losses FROM users ORDER BY elo_rating DESC LIMIT $1;", limit);
    txn.commit();

    std::vector<UserProfile> leaderboard;
    leaderboard.reserve(result.size());
    for (const auto& row : result) {
        leaderboard.push_back(
            UserProfile{row[0].as<std::string>(), row[1].as<int>(), row[2].as<int>(), row[3].as<int>()});
    }
    return leaderboard;
}

}  // namespace kungfu
