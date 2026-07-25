#pragma once

#include <string>

#include "Room.hpp"
#include "RoomManager.hpp"

#include "Network/Protocol.hpp"

namespace kungfu {

// Routes a decoded CLICK message from an already-joined connection to its Controller. A
// no-op (does nothing) if: the connection isn't actually routed to a room (rejected or
// unrecognized), it's a spectator (or a forfeited match's former player) rather than a
// seated player, the message isn't really a click, or - the one rule that doesn't exist
// locally today (one mouse could always move either color) - it's trying to *select* a
// piece that isn't its own color (once a selection is already active, the second click -
// the actual move/jump target - goes straight through without this check).
//
// Not thread-safe on its own: assumes Server/main.cpp's gameMutex is already held by the
// caller, same contract as RoomManager/Outbox/ConnectionSessions.
void handleClick(const std::string& connectionId, const net::DecodedMessage& decoded, RoomManager& roomManager);

}  // namespace kungfu
