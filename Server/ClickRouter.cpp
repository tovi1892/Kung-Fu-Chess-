#include "ClickRouter.hpp"

#include <variant>

#include "model/Position.hpp"

namespace kungfu {

void handleClick(Room& room, const std::string& connectionId, const net::DecodedMessage& decoded) {
    const auto sessionIt = room.players.find(connectionId);
    if (sessionIt == room.players.end()) {
        return;  // a spectator (or a forfeited match's former players) - clicks do nothing
    }

    const auto* click = std::get_if<net::ClickMessage>(&decoded);
    if (!click) {
        return;
    }

    PlayerSession& session = sessionIt->second;
    const Position cell(click->row, click->col);

    // The one rule that doesn't exist locally today (one mouse could always move either
    // color): a connection may only ever *select* its own color's pieces. Once a selection
    // is active, it was already validated here, so the second click (the actual move/jump
    // target) can go straight through.
    if (!session.controller->hasSelection()) {
        const auto piece = room.game->getBoard()->pieceAt(cell);
        if (!piece.has_value() || !piece.value() || piece.value()->color() != session.color) {
            return;
        }
    }

    session.controller->handleCellClick(cell.row(), cell.col());
}

}  // namespace kungfu
