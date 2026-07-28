#pragma once

#include <optional>
#include <string>

namespace kungfu {

// Abstraction boundary between the Gateway's cross-shard routing and whatever actually
// stores the room->shard mapping - mirrors Server/IAccountRepository.hpp's role exactly.
//
// Only ever consulted for a *freeform* JoinRoom key - one a client typed without ever having
// received it from a CreateRoom ROOM reply (see WsGateway.hpp's key-prefix scheme). A
// CreateRoom-issued key is routed by parsing its shard-index prefix alone, no lookup needed;
// this registry exists purely for the secondary case of two clients manually agreeing on a
// made-up room name with no Gateway-issued code involved.
class IRoomRegistry {
public:
    virtual ~IRoomRegistry() = default;

    // nullopt if roomKey has never been registered.
    virtual std::optional<std::string> lookup(const std::string& roomKey) = 0;

    // Idempotent for a repeat of the same (roomKey, shardUrl) pair. Last-write-wins if ever
    // called again with a different shardUrl for the same key - two different clients typing
    // the identical freeform room name on different shards independently is a real,
    // documented (not fixed) rough edge for this slice.
    virtual void registerRoom(const std::string& roomKey, const std::string& shardUrl) = 0;
};

}  // namespace kungfu
