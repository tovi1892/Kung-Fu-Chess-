#include "MatchmakerProtocol.hpp"

#include <exception>

#include "io/BoardParser.hpp"

namespace kungfu::net::mm {

std::string encodeWait(const std::string& username, int rating) {
    return "WAIT " + username + " " + std::to_string(rating);
}

std::string encodeMatched(std::size_t shardIndex, const std::string& roomKey) {
    return "MATCHED " + std::to_string(shardIndex) + " " + roomKey;
}

std::string encodeNoOpponent() {
    return "NO_OPPONENT";
}

DecodedMmMessage decode(const std::string& text) {
    const auto tokens = BoardParser::split(text);
    if (tokens.empty()) {
        return std::monostate{};
    }
    const std::string& cmd = tokens[0];

    if (cmd == "WAIT" && tokens.size() >= 3) {
        try {
            return WaitMessage{tokens[1], std::stoi(tokens[2])};
        } catch (const std::exception&) {
            return std::monostate{};
        }
    }

    if (cmd == "MATCHED" && tokens.size() >= 3) {
        try {
            return MatchedMessage{static_cast<std::size_t>(std::stoul(tokens[1])), tokens[2]};
        } catch (const std::exception&) {
            return std::monostate{};
        }
    }

    if (cmd == "NO_OPPONENT") {
        return NoOpponentMessage{};
    }

    return std::monostate{};
}

}  // namespace kungfu::net::mm
