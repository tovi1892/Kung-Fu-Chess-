#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "Room.hpp"

#include "Network/WsServerTransport.hpp"

namespace kungfu {

// Buffers messages queued while some other lock (the registry mutex, or a specific room's
// roomMutex) is held, sending them only once flush() is called - preserves the reasoning
// already documented for those locks: sending directly while holding one risked a
// self-deadlock (ix::WebSocket::send() can synchronously invoke this transport's Close
// callback on the same thread, which tries to relock the very mutex the send happened
// under).
//
// Owns a small private mutex of its own, rather than reusing any single caller-held lock -
// necessary now that callers hold one of several different mutexes (the registry mutex for
// cross-room operations, or any one of many rooms' own roomMutex for room-scoped ones):
// enqueue()/enqueueToRoom() can be called concurrently from different rooms' independently-
// held locks, so pending_ needs its own synchronization independent of all of them. The
// critical section is just a vector swap - never held while actually sending - so this adds
// no meaningful contention of its own.
class Outbox {
public:
    explicit Outbox(net::WsServerTransport& server) : server_(server) {}

    // Safe to call from any thread, under whatever lock (if any) that thread already holds -
    // just records intent, sends nothing.
    void enqueue(const net::WsServerTransport::ConnectionId& id, const std::string& text);

    // Reads room.players/room.spectators - the caller must already hold room.roomMutex
    // (Outbox has no way to know which Room's lock that is, so it can't take it itself).
    void enqueueToRoom(const Room& room, const std::string& text);

    // Briefly locks this Outbox's own mutex just long enough to swap out whatever was
    // queued, then sends every message with no lock held at all.
    void flush();

private:
    net::WsServerTransport& server_;
    std::mutex mutex_;
    std::vector<std::pair<net::WsServerTransport::ConnectionId, std::string>> pending_;
};

}  // namespace kungfu
