#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Room.hpp"

namespace kungfu {

// A connection searching for a quick-match opponent, waiting to be paired or to time out.
// Connection ids are plain strings (matching Room.hpp's own convention) - RoomManager stays
// pure Logic/Server-composition, with zero networking awareness.
struct WaitingPlayer {
    std::string id;
    std::string username;
    int rating;
    std::chrono::steady_clock::time_point deadline;
};

// Owns every Room ever created (never removed - see the Phase 4 plan's non-goals) and the
// quick-match waiting list. It creates/looks up rooms and pairs/queues/expires quick-match
// waiters - it does not touch sockets, parse protocol messages, send anything, or decide
// what to do with a match once found or a room once created (that stays in Server/main.cpp).
//
// Not thread-safe on its own: every method assumes the caller already holds
// Server/main.cpp's gameMutex, exactly as direct access to the old rooms_/
// quickMatchWaiting_ locals did before this class existed - it introduces no locking of its
// own and changes no ordering guarantees.
class RoomManager {
public:
    // Returns the existing room for `key`, or creates+stores a fresh one (via Room.hpp's
    // createRoom()) and returns that. onCreated, if given, fires exactly once - only when a
    // room is actually newly created, before it's returned - so callers can wire up
    // per-room setup (e.g. Server/main.cpp's registerRoomBroadcasts) without RoomManager
    // needing to know anything about GameEngine's event buses.
    Room& getOrCreateRoom(const std::string& key, const std::function<void(Room&)>& onCreated = {});

    // Every room ever created, keyed by room id. Exposed directly (not wrapped) for now -
    // Server/main.cpp's tick loop (STATE broadcast, forfeit-deadline sweep) and
    // connection-routing lookups still need to reach into individual Room contents that
    // haven't moved into RoomManager yet.
    std::unordered_map<std::string, std::unique_ptr<Room>>& rooms();

    // Plain incrementing ids for CreateRoom / quick-match room keys respectively - never
    // repeat within the process's lifetime, so no uniqueness check is needed (see
    // Server/main.cpp's original comments: one's meant to be short/typeable, the other's
    // never shown to a client at all).
    std::string allocateNamedRoomKey();
    std::string allocateQuickMatchRoomKey();

    // Quick-match pairing. eloRange is passed in (not stored) - RoomManager owns the
    // waiting-list mechanics, not the matchmaking policy constant.
    // Returns and removes the first waiter within eloRange of `rating`, if any.
    std::optional<WaitingPlayer> matchWaiter(int rating, int eloRange);
    void addWaiter(WaitingPlayer waiter);
    // Removes any waiter for this connection id, if present (a no-op otherwise) - used both
    // by disconnect cleanup and (indirectly) by reapExpiredWaiters below.
    void removeWaiter(const std::string& connectionId);
    // Removes and returns every waiter whose deadline has passed as of `now` - the caller
    // still owns sending a no-opponent-found reply and logging for each.
    std::vector<WaitingPlayer> reapExpiredWaiters(std::chrono::steady_clock::time_point now);

    // Result of a successful reconnect() match: which room the seat lives in, and which
    // color was reclaimed. `room` is a raw pointer (not a reference) only because
    // std::optional<T&> isn't available pre-C++26 - never null when the optional itself
    // holds a value.
    struct ReconnectResult {
        Room* room;
        PlayerColor color;
    };

    // Scans every room for one with a pending forfeit whose disconnected seat's username
    // matches `username`; if found, reseats that PlayerSession under `newConnectionId`
    // (moving it out of its old connection-id key), clears the pending forfeit, and returns
    // the room + reclaimed color. Returns nullopt if no room currently has a matching
    // pending-forfeit seat. Pure state mutation only - never sends anything (the caller
    // still owns queueSend/queueToRoom/logging for whatever this returns), same split as
    // matchWaiter()/addWaiter() above.
    std::optional<ReconnectResult> reconnect(const std::string& username, const std::string& newConnectionId);

    // Records that `connectionId` is now routed to (seated in or spectating) room
    // `roomKey`.
    void assignConnectionToRoom(const std::string& connectionId, const std::string& roomKey);

    // True if this connection is currently routed to some room - used to gate JOIN
    // handling (an already-routed connection's next message is a click, not another JOIN).
    bool isConnectionRouted(const std::string& connectionId) const;

    // The Room this connection is currently routed to, or nullptr if it isn't routed to
    // any - same Room*-not-a-reference shape as ReconnectResult::room, for the same reason
    // (never null when a room is actually found).
    Room* roomForConnection(const std::string& connectionId) const;

    // Forgets this connection's room assignment - a no-op if it wasn't assigned to one.
    // Does NOT touch Room::players/spectators itself - callers (e.g. onDisconnect) still
    // own deciding what happens to the vacated seat.
    void forgetConnectionRoom(const std::string& connectionId);

    // Starts a forfeit grace period for `disconnectedColor` in `room`, expiring `graceMs`
    // from now.
    void startForfeitCountdown(Room& room, PlayerColor disconnectedColor, int graceMs);

    // If `room`'s pendingForfeit has expired as of `now`, resolves it: computes the winner,
    // moves every remaining player into spectators (keeps receiving STATE) and clears the
    // players map, stops the room's game (GameEngine has no idea a forfeit happened - no
    // king was captured - so without this it would happily keep running and could later
    // fire a second, conflicting GameEnded), and clears pendingForfeit. Returns the winner
    // if it resolved anything, nullopt otherwise (including "no pending forfeit at all").
    //
    // Deliberately takes one Room at a time and is meant to be called from within
    // Server/main.cpp's existing per-room tick loop, immediately before that same room's
    // game->wait(kTickMs) - NOT a separate up-front pass like reapExpiredWaiters() above -
    // preserving the exact same-tick, same-room stop()-before-wait() ordering the original
    // inline code relied on (a resolved forfeit's game->stop() must run before this tick's
    // wait() for the same room, or GameEngine would advance one extra tick of simulation
    // after the deadline passed).
    std::optional<PlayerColor> resolveForfeitIfExpired(Room& room, std::chrono::steady_clock::time_point now);

private:
    std::unordered_map<std::string, std::unique_ptr<Room>> rooms_;
    std::vector<WaitingPlayer> quickMatchWaiting_;
    int nextQuickMatchId_ = 0;
    int nextNamedRoomId_ = 0;
    std::unordered_map<std::string, std::string> connectionRoom_;
};

}  // namespace kungfu
