#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "engine/GameEngine.hpp"
#include "input/Controller.hpp"
#include "io/BoardParser.hpp"
#include "model/Board.hpp"
#include "rules/RuleEngine.hpp"

#include "Network/Protocol.hpp"
#include "Network/WsServerTransport.hpp"

using namespace kungfu;
using namespace kungfu::net;

namespace {

// Same starting position as the local client's main.cpp used to build directly - the
// server is now the only place that does this.
const char* kInitialBoard = R"(Board:
wR wN wB wQ wK wB wN wR
wP wP wP wP wP wP wP wP
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .
bP bP bP bP bP bP bP bP
bR bN bB bQ bK bB bN bR
)";

constexpr int kPort = 7777;
constexpr int kTickMs = 16;

// One connected, *joined* player: their username, which color they were assigned, and
// their own Controller. Both players' Controllers wrap the *same* shared GameEngine -
// Controller was already designed to own nothing but its own selection state, so this
// needs zero Logic/ changes.
struct PlayerSession {
    std::string username;
    PlayerColor color;
    std::shared_ptr<Controller> controller;
};

}  // namespace

int main() {
    std::shared_ptr<Board> board;
    std::istringstream boardStream(kInitialBoard);
    if (!BoardParser().parseInput(boardStream, board)) {
        std::cerr << "Failed to parse the initial board layout" << std::endl;
        return 1;
    }

    auto ruleEngine = std::make_shared<RuleEngine>(board);
    auto game = std::make_shared<GameEngine>(board, ruleEngine);

    // Guards every access to game/sessions/pendingConnections below - IXWebSocket
    // delivers connect/message/disconnect callbacks on its own background I/O thread(s),
    // while GameEngine and Controller were built single-threaded (see the Phase 1 plan's
    // mutex note).
    std::mutex gameMutex;

    // A connection lives here from the moment it opens until its JOIN <username> arrives
    // (or it disconnects first) - it doesn't get a color or a Controller, and its clicks
    // are ignored, until it has actually joined. Kept separate from `sessions` so a slow
    // or silent connection still correctly reserves one of the two player slots.
    std::unordered_set<WsServerTransport::ConnectionId> pendingConnections;
    std::unordered_map<WsServerTransport::ConnectionId, PlayerSession> sessions;

    WsServerTransport server(kPort);

    auto broadcastState = [&]() {
        std::vector<RenderPiece> state;
        {
            std::lock_guard<std::mutex> lock(gameMutex);
            state = game->getRenderState();
        }
        server.broadcast(encodeState(state));
    };

    server.onConnect([&](const WsServerTransport::ConnectionId& id) {
        std::lock_guard<std::mutex> lock(gameMutex);

        if (sessions.size() + pendingConnections.size() >= 2) {
            std::cout << "[server] rejecting extra connection " << id
                      << " (spectators are a later phase)" << std::endl;
            server.close(id);
            return;
        }

        pendingConnections.insert(id);
        std::cout << "[server] connection " << id << " opened - waiting for JOIN" << std::endl;
    });

    server.onMessage([&](const WsServerTransport::ConnectionId& id, const std::string& text) {
        std::lock_guard<std::mutex> lock(gameMutex);

        const auto decoded = decode(text);

        // Still waiting to join: the only message that means anything from this
        // connection is JOIN - anything else (e.g. a stray click before the username
        // window closed) is ignored.
        if (pendingConnections.count(id) > 0) {
            const auto* join = std::get_if<JoinMessage>(&decoded);
            if (!join) {
                return;
            }

            pendingConnections.erase(id);
            const PlayerColor color = sessions.empty() ? PlayerColor::White : PlayerColor::Black;
            sessions[id] = PlayerSession{join->username, color, std::make_shared<Controller>(game)};
            std::cout << "[server] " << join->username << " joined as "
                      << (color == PlayerColor::White ? "White" : "Black") << std::endl;
            server.send(id, encodeWelcome(color));

            if (sessions.size() == 2) {
                std::string whiteName, blackName;
                for (const auto& [sessionId, session] : sessions) {
                    (session.color == PlayerColor::White ? whiteName : blackName) = session.username;
                }
                server.broadcast(encodePlayers(whiteName, blackName));
                game->start();
                std::cout << "[server] both players joined (" << whiteName << " vs " << blackName
                          << ") - game started" << std::endl;
            }
            return;
        }

        // Already joined: the only message a joined connection ever sends is a click.
        auto it = sessions.find(id);
        if (it == sessions.end()) {
            return;  // a rejected/unrecognized connection - ignore whatever it sends
        }

        const auto* click = std::get_if<ClickMessage>(&decoded);
        if (!click) {
            return;
        }

        PlayerSession& session = it->second;
        const Position cell(click->row, click->col);

        // The one rule that doesn't exist locally today (one mouse could always move
        // either color): a connection may only ever *select* its own color's pieces.
        // Once a selection is active, it was already validated here, so the second
        // click (the actual move/jump target) can go straight through.
        if (!session.controller->hasSelection()) {
            const auto piece = game->getBoard()->pieceAt(cell);
            if (!piece.has_value() || !piece.value() || piece.value()->color() != session.color) {
                return;
            }
        }

        session.controller->handleCellClick(cell.row(), cell.col());
    });

    server.onDisconnect([&](const WsServerTransport::ConnectionId& id) {
        std::lock_guard<std::mutex> lock(gameMutex);
        pendingConnections.erase(id);
        sessions.erase(id);
        std::cout << "[server] connection " << id << " disconnected" << std::endl;
    });

    // Broadcast every bus event to both connections the instant it happens.
    game->onMoveStarted().subscribe(
        [&](const MoveStarted& event) { server.broadcast(encodeMoveStarted(event)); });
    game->onPieceCaptured().subscribe(
        [&](const PieceCaptured& event) { server.broadcast(encodePieceCaptured(event)); });
    game->onGameStarted().subscribe([&](const GameStarted&) { server.broadcast(encodeGameStarted()); });
    game->onGameEnded().subscribe(
        [&](const GameEnded& event) { server.broadcast(encodeGameEnded(event)); });

    server.start();
    std::cout << "[server] listening on port " << kPort << " - waiting for 2 players..." << std::endl;

    while (true) {
        {
            std::lock_guard<std::mutex> lock(gameMutex);
            game->wait(kTickMs);  // a no-op until both players have connected and game->start() ran
        }
        broadcastState();
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
    }
}
