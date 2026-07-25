#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "Room.hpp"

#include "Network/WsServerTransport.hpp"

namespace kungfu {

// Buffers messages queued while Server/main.cpp's gameMutex is held, sending them only
// once flush() is called after the caller has released that lock - preserves the exact
// reasoning already documented in main.cpp: sending directly while holding gameMutex risked
// a self-deadlock (ix::WebSocket::send() can synchronously invoke this transport's Close
// callback on the same thread, which tries to relock gameMutex inside onDisconnect).
//
// Not thread-safe on its own: enqueue()/enqueueToRoom() assume gameMutex is already held by
// the caller (same contract RoomManager already established). flush() assumes the OPPOSITE
// (gameMutex NOT held) and briefly re-locks the exact same mutex instance given at
// construction just long enough to swap out whatever's queued, then sends with no lock held
// at all. Deliberately reuses main()'s one gameMutex rather than owning a private mutex of
// its own - a dedicated mutex was considered and explicitly rejected, since it would be a
// real (if small) behavioral change this refactor isn't meant to make.
class Outbox {
public:
    Outbox(net::WsServerTransport& server, std::mutex& gameMutex) : server_(server), gameMutex_(gameMutex) {}

    // Assumes gameMutex is already held by the caller - just records intent, sends nothing.
    void enqueue(const net::WsServerTransport::ConnectionId& id, const std::string& text);
    void enqueueToRoom(const Room& room, const std::string& text);

    // Called with gameMutex NOT held - briefly re-locks it just long enough to swap out
    // whatever was queued, then sends every message with no lock held at all.
    void flush();

private:
    net::WsServerTransport& server_;
    std::mutex& gameMutex_;
    std::vector<std::pair<net::WsServerTransport::ConnectionId, std::string>> pending_;
};

}  // namespace kungfu
