#include "MatchResult.hpp"

#include "Network/Protocol.hpp"

namespace kungfu {

void applyMatchResult(Room& room, PlayerColor winnerColor, IAccountRepository& accounts, Outbox& outbox, net::Logger& logger) {
    std::string winnerName, loserName;
    for (const auto& [id, session] : room.players) {
        (void)id;
        if (session.color == winnerColor) {
            winnerName = session.username;
        } else {
            loserName = session.username;
        }
    }
    if (winnerName.empty() || loserName.empty()) {
        return;  // shouldn't happen - defensive, not a real expected path
    }

    const auto elo = accounts.recordResult(winnerName, loserName);
    const int whiteRating = winnerColor == PlayerColor::White ? elo.newWinnerRating : elo.newLoserRating;
    const int blackRating = winnerColor == PlayerColor::White ? elo.newLoserRating : elo.newWinnerRating;
    outbox.enqueueToRoom(room, net::encodeRatings(whiteRating, blackRating));
    logger.log("room \"" + room.key + "\": rating update " + winnerName + " -> " +
               std::to_string(elo.newWinnerRating) + ", " + loserName + " -> " +
               std::to_string(elo.newLoserRating));
}

}  // namespace kungfu
