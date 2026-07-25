#pragma once

#include "AccountStore.hpp"
#include "Outbox.hpp"
#include "Room.hpp"

#include "Network/Logger.hpp"

namespace kungfu {

// Applies the standard Elo update for a concluded match (a real king capture's GameEnded,
// or a disconnect countdown elapsing) and broadcasts the resulting ratings to the room via
// outbox. Looks up both players' usernames from room.players - must still be present, so
// callers should run this before any post-match player/spectator bookkeeping (e.g. a
// forfeit's move-everyone-to-spectators step). A no-op if either username can't be found -
// shouldn't happen, defensive only.
//
// Not thread-safe on its own: assumes Server/main.cpp's gameMutex is already held by the
// caller, same contract as RoomManager/Outbox/ConnectionSessions.
void applyMatchResult(Room& room, PlayerColor winnerColor, AccountStore& accounts, Outbox& outbox, net::Logger& logger);

}  // namespace kungfu
