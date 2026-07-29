#include "SqliteAccountRepository.hpp"

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

// Client-generated UUID v4 (RFC 4122) - SQLite has no built-in UUID generator, and this is a
// local dev-only artifact anyway (see PostgresAccountRepository.cpp's equivalent for the
// networked-backend rationale).
std::string generateUuidV4() {
    auto bytes = randomBytes(16);
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
    const std::string hex = toHex(bytes);
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) + "-" + hex.substr(16, 4) +
           "-" + hex.substr(20, 12);
}

}  // namespace

SqliteAccountRepository::SqliteAccountRepository(const std::string& dbPath) {
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
    // Local-dev counterpart to init-db.sql's `matches` table - references usernames directly
    // rather than a surrogate user_id, matching `accounts`' own username-as-primary-key shape.
    execOrThrow(db_, R"(
        CREATE TABLE IF NOT EXISTS matches (
            match_id TEXT PRIMARY KEY,
            white_username TEXT NOT NULL,
            black_username TEXT NOT NULL,
            winner_username TEXT NOT NULL,
            white_elo_delta INTEGER NOT NULL,
            black_elo_delta INTEGER NOT NULL,
            pgn_data TEXT,
            ended_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )");
}

SqliteAccountRepository::~SqliteAccountRepository() {
    if (db_) {
        sqlite3_close(db_);
    }
}

LoginResult SqliteAccountRepository::login(const std::string& username, const std::string& password) {
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

EloUpdateResult SqliteAccountRepository::recordResult(const MatchRecord& match) {
    execOrThrow(db_, "BEGIN;");

    const bool whiteIsWinner = match.winnerUsername == match.whiteUsername;
    const std::string& winnerUsername = match.winnerUsername;
    const std::string& loserUsername = whiteIsWinner ? match.blackUsername : match.whiteUsername;

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

    const int whiteEloDelta = whiteIsWinner ? (newWinnerRating - winnerRating) : (newLoserRating - loserRating);
    const int blackEloDelta = whiteIsWinner ? (newLoserRating - loserRating) : (newWinnerRating - winnerRating);

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                        "INSERT INTO matches (match_id, white_username, black_username, winner_username, "
                        "white_elo_delta, black_elo_delta, pgn_data) VALUES (?, ?, ?, ?, ?, ?, ?);",
                        -1, &stmt, nullptr);
    const std::string matchId = generateUuidV4();
    sqlite3_bind_text(stmt, 1, matchId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, match.whiteUsername.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, match.blackUsername.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, match.winnerUsername.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, whiteEloDelta);
    sqlite3_bind_int(stmt, 6, blackEloDelta);
    sqlite3_bind_text(stmt, 7, match.pgnData.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    execOrThrow(db_, "COMMIT;");

    return EloUpdateResult{newWinnerRating, newLoserRating};
}

}  // namespace kungfu
