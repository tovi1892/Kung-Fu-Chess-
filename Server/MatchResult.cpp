#include "MatchResult.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

#include "history/GameRecord.hpp"

#include "Network/Protocol.hpp"

namespace kungfu {

namespace {

// Chronological merge of both colors' recorded moves into one loose move log - not a strict
// PGN export (this variant has no turns to pair moves into "1. e4 e5" numbering, since either
// side can move at any moment - see RealTimeArbiter), just enough move-by-move text to be
// useful in the `matches.pgn_data` column.
std::string buildMoveLog(const GameRecord& record) {
    struct Entry {
        int elapsedMs;
        PlayerColor color;
        std::string notation;
    };
    std::vector<Entry> entries;
    for (const auto& move : record.movesFor(PlayerColor::White)) {
        entries.push_back({move.elapsedMs, PlayerColor::White, move.notation});
    }
    for (const auto& move : record.movesFor(PlayerColor::Black)) {
        entries.push_back({move.elapsedMs, PlayerColor::Black, move.notation});
    }
    std::stable_sort(entries.begin(), entries.end(),
                      [](const Entry& a, const Entry& b) { return a.elapsedMs < b.elapsedMs; });

    std::ostringstream out;
    for (const auto& entry : entries) {
        out << (entry.color == PlayerColor::White ? "W:" : "B:") << entry.notation << " ";
    }
    return out.str();
}

}  // namespace

void applyMatchResult(Room& room, PlayerColor winnerColor, IAccountRepository& accounts, Outbox& outbox, net::Logger& logger) {
    std::string whiteName, blackName;
    for (const auto& [id, session] : room.players) {
        (void)id;
        if (session.color == PlayerColor::White) {
            whiteName = session.username;
        } else {
            blackName = session.username;
        }
    }
    if (whiteName.empty() || blackName.empty()) {
        return;  // shouldn't happen - defensive, not a real expected path
    }
    const std::string& winnerName = winnerColor == PlayerColor::White ? whiteName : blackName;
    const std::string& loserName = winnerColor == PlayerColor::White ? blackName : whiteName;

    const MatchRecord match{whiteName, blackName, winnerName, buildMoveLog(room.game->gameRecord())};
    const auto elo = accounts.recordResult(match);
    const int whiteRating = winnerColor == PlayerColor::White ? elo.newWinnerRating : elo.newLoserRating;
    const int blackRating = winnerColor == PlayerColor::White ? elo.newLoserRating : elo.newWinnerRating;
    outbox.enqueueToRoom(room, net::encodeRatings(whiteRating, blackRating));
    logger.log("room \"" + room.key + "\": rating update " + winnerName + " -> " +
               std::to_string(elo.newWinnerRating) + ", " + loserName + " -> " +
               std::to_string(elo.newLoserRating));
}

}  // namespace kungfu
