#pragma once

#include <cstddef>
#include <string>
#include <variant>

namespace kungfu::net::mm {

// The internal wire protocol between the WS Gateway and the Matchmaker service - entirely
// separate from Network/Protocol.hpp's client-facing game protocol (own namespace so its
// NoOpponentMessage/encodeNoOpponent() can't be confused with that protocol's identically-
// named client-facing ones; these are two different protocols that happen to share a word).
// Same hand-rolled space-delimited style as Protocol.hpp.

// Gateway -> Matchmaker: this connection is searching for a quick-match opponent. No
// connection-id field - the Matchmaker's notion of "which search" is simply whichever socket
// this arrived on; replies go back on that same socket.
struct WaitMessage {
    std::string username;
    int rating;
};

// Matchmaker -> Gateway: a pair formed. shardIndex is this Gateway's own index into its
// locally-configured shard list - the Matchmaker never learns real shard URLs, only a count.
// roomKey is the raw (unprefixed) "quickmatch-N" key, globally unique.
struct MatchedMessage {
    std::size_t shardIndex;
    std::string roomKey;
};

// Matchmaker -> Gateway: this waiter's deadline passed with nobody found.
struct NoOpponentMessage {};

using DecodedMmMessage = std::variant<std::monostate, WaitMessage, MatchedMessage, NoOpponentMessage>;

std::string encodeWait(const std::string& username, int rating);
std::string encodeMatched(std::size_t shardIndex, const std::string& roomKey);
std::string encodeNoOpponent();

// Returns std::monostate (index 0) for anything unrecognized or malformed.
DecodedMmMessage decode(const std::string& text);

}  // namespace kungfu::net::mm
