#pragma once

#include <mutex>
#include <string>

#include "IRoomRegistry.hpp"

// hiredis's redisContext is a real struct tag (`typedef struct redisContext redisContext;`),
// unlike libpqxx's pqxx::connection type-alias problem documented in
// Server/PostgresAccountRepository.hpp - a plain forward declaration works fine here, so
// <hiredis/hiredis.h> only needs to be included in the .cpp.
struct redisContext;

namespace kungfu {

// Redis-backed IRoomRegistry (see IRoomRegistry.hpp for what it's actually used for - only
// freeform JoinRoom keys, not CreateRoom's shard-index-prefixed ones). Compiled only when
// KUNGFU_ENABLE_REDIS is on (see CMakeLists.txt) - the native Windows dev build never needs
// hiredis installed at all, matching PostgresAccountRepository's KUNGFU_ENABLE_POSTGRES gate.
//
// Key scheme: "room:" + roomKey -> shardUrl, no TTL - rooms live forever today per
// RoomManager, so this matches existing behavior rather than inventing expiry semantics.
//
// A single redisContext isn't safe to share across threads without external synchronization,
// same caveat PostgresAccountRepository.hpp documents for pqxx::connection - guarded here by
// a private mutex, since multiple downstream connections' IXWebSocket threads can call
// lookup()/registerRoom() concurrently.
class RedisRoomRegistry : public IRoomRegistry {
public:
    // Throws std::runtime_error if the connection fails.
    RedisRoomRegistry(const std::string& host, int port);
    ~RedisRoomRegistry() override;

    RedisRoomRegistry(const RedisRoomRegistry&) = delete;
    RedisRoomRegistry& operator=(const RedisRoomRegistry&) = delete;

    std::optional<std::string> lookup(const std::string& roomKey) override;
    void registerRoom(const std::string& roomKey, const std::string& shardUrl) override;

private:
    redisContext* ctx_;
    std::mutex mutex_;
};

}  // namespace kungfu
