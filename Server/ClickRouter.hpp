#pragma once

#include <string>

#include "Room.hpp"

#include "Network/Protocol.hpp"

namespace kungfu {

// Routes a decoded CLICK message from an already-joined connection to its Controller. A
// no-op (does nothing) if: it's a spectator (or a forfeited match's former player) rather
// than a seated player, the message isn't really a click, or - the one rule that doesn't
// exist locally today (one mouse could always move either color) - it's trying to *select*
// a piece that isn't its own color (once a selection is already active, the second click -
// the actual move/jump target - goes straight through without this check).
//
// Takes the target Room directly rather than looking it up itself: GameServer resolves
// connectionId -> Room under the registry mutex and releases it before calling here, so this
// function never needs RoomManager at all. Not thread-safe on its own: assumes the caller
// already holds room.roomMutex.
void handleClick(Room& room, const std::string& connectionId, const net::DecodedMessage& decoded);

}  // namespace kungfu
