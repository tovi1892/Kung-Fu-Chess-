#include "AccountStore.hpp"

#include <windows.h>

#include <bcrypt.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include <sqlite3.h>

namespace kungfu {

namespace {

constexpr int kSaltLength = 16;
constexpr ULONG kHashLength = 32;  // SHA-256 output size
constexpr ULONG kPbkdf2Iterations = 100000;
constexpr int kStartingRating = 1200;
constexpr int kEloKFactor = 32;

// --- Password hashing (BCryptGenRandom/BCryptDeriveKeyPBKDF2, Windows CNG) -------------
// Native OS API, not a new third-party crypto library - same "reach for bcrypt.dll/winmm
// instead of a new dependency" pattern this project already uses for sound/sockets.

std::vector<unsigned char> randomBytes(int count) {
    std::vector<unsigned char> buffer(count);
    const NTSTATUS status =
        BCryptGenRandom(nullptr, buffer.data(), static_cast<ULONG>(buffer.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
    return buffer;
}

std::vector<unsigned char> derivePbkdf2(const std::string& password, const std::vector<unsigned char>& salt) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS status =
        BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
    }

    std::vector<unsigned char> derived(kHashLength);
    status = BCryptDeriveKeyPBKDF2(hAlg, reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
                                    static_cast<ULONG>(password.size()), const_cast<PUCHAR>(salt.data()),
                                    static_cast<ULONG>(salt.size()), kPbkdf2Iterations, derived.data(),
                                    static_cast<ULONG>(derived.size()), 0);

    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("BCryptDeriveKeyPBKDF2 failed");
    }
    return derived;
}

// --- Small SQLite helpers --------------------------------------------------------------

void execOrThrow(sqlite3* db, const char* sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        const std::string message = errMsg ? errMsg : "unknown SQLite error";
        sqlite3_free(errMsg);
        throw std::runtime_error("SQLite error: " + message);
    }
}

int getRating(sqlite3* db, const std::string& username) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT rating FROM accounts WHERE username = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    int rating = kStartingRating;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rating = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return rating;
}

void setRating(sqlite3* db, const std::string& username, int rating) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "UPDATE accounts SET rating = ? WHERE username = ?;", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, rating);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

}  // namespace

AccountStore::AccountStore(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite database: " + dbPath);
    }
    execOrThrow(db_, R"(
        CREATE TABLE IF NOT EXISTS accounts (
            username TEXT PRIMARY KEY,
            password_hash BLOB NOT NULL,
            password_salt BLOB NOT NULL,
            rating INTEGER NOT NULL DEFAULT 1200
        );
    )");
}

AccountStore::~AccountStore() {
    if (db_) {
        sqlite3_close(db_);
    }
}

LoginResult AccountStore::login(const std::string& username, const std::string& password) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT password_hash, password_salt, rating FROM accounts WHERE username = ?;", -1,
                        &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* hashBlob = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, 0));
        const int hashLen = sqlite3_column_bytes(stmt, 0);
        const auto* saltBlob = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, 1));
        const int saltLen = sqlite3_column_bytes(stmt, 1);
        const int rating = sqlite3_column_int(stmt, 2);

        const std::vector<unsigned char> storedHash(hashBlob, hashBlob + hashLen);
        const std::vector<unsigned char> salt(saltBlob, saltBlob + saltLen);
        sqlite3_finalize(stmt);

        if (derivePbkdf2(password, salt) == storedHash) {
            return LoginResult{true, "", rating, false};
        }
        return LoginResult{false, "bad_password", 0, false};
    }
    sqlite3_finalize(stmt);

    // Unknown username - auto-register a fresh account rather than requiring a separate
    // sign-up step (see the Phase 4 plan's explicit non-goals).
    const auto salt = randomBytes(kSaltLength);
    const auto hash = derivePbkdf2(password, salt);

    sqlite3_stmt* insertStmt = nullptr;
    sqlite3_prepare_v2(db_, "INSERT INTO accounts (username, password_hash, password_salt, rating) VALUES (?, ?, ?, ?);",
                        -1, &insertStmt, nullptr);
    sqlite3_bind_text(insertStmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(insertStmt, 2, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(insertStmt, 3, salt.data(), static_cast<int>(salt.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(insertStmt, 4, kStartingRating);
    sqlite3_step(insertStmt);
    sqlite3_finalize(insertStmt);

    return LoginResult{true, "", kStartingRating, true};
}

EloUpdateResult AccountStore::recordResult(const std::string& winnerUsername, const std::string& loserUsername) {
    execOrThrow(db_, "BEGIN;");

    const int winnerRating = getRating(db_, winnerUsername);
    const int loserRating = getRating(db_, loserUsername);

    // Standard Elo: expected score from the rating gap, then move the actual result (1 for
    // the winner, 0 for the loser - this variant has no draws) toward that expectation,
    // scaled by K. The loser's new rating is derived from the winner's actual delta so the
    // total rating "moved" between the two accounts is exactly conserved.
    const double expectedWinner = 1.0 / (1.0 + std::pow(10.0, (loserRating - winnerRating) / 400.0));
    const int newWinnerRating = static_cast<int>(std::lround(winnerRating + kEloKFactor * (1.0 - expectedWinner)));
    const int newLoserRating = loserRating - (newWinnerRating - winnerRating);

    setRating(db_, winnerUsername, newWinnerRating);
    setRating(db_, loserUsername, newLoserRating);

    execOrThrow(db_, "COMMIT;");

    return EloUpdateResult{newWinnerRating, newLoserRating};
}

}  // namespace kungfu
