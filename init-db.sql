-- Initial PostgreSQL Schema for Kung-Fu Chess Cloud Migration

-- password_hash/password_salt store the same PBKDF2-HMAC-SHA256 (100000 iterations) scheme
-- SqliteAccountRepository already uses, hex-encoded as TEXT rather than BYTEA -
-- PostgresAccountRepository.cpp encodes/decodes the hex itself, using only libpqxx's plain
-- std::string parameter/result path (the most stable part of its API across versions,
-- deliberately avoided the binary-column API here since this couldn't be compile-tested
-- against a real libpqxx locally - see Server/PostgresAccountRepository.cpp).
CREATE TABLE IF NOT EXISTS users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    password_salt TEXT NOT NULL,
    elo_rating INT DEFAULT 1200,
    wins INT DEFAULT 0,
    losses INT DEFAULT 0,
    draws INT DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS matches (
    match_id UUID PRIMARY KEY,
    white_user_id INT REFERENCES users(user_id),
    black_user_id INT REFERENCES users(user_id),
    winner_user_id INT REFERENCES users(user_id),
    white_elo_delta INT NOT NULL,
    black_elo_delta INT NOT NULL,
    pgn_data TEXT,
    started_at TIMESTAMP WITH TIME ZONE,
    ended_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_matches_white_user ON matches(white_user_id);
CREATE INDEX IF NOT EXISTS idx_matches_black_user ON matches(black_user_id);