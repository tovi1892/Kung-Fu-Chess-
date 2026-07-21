#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "history/GameRecord.hpp"
#include "model/Enums.hpp"
#include "model/Position.hpp"

#include "Client/RemoteGameProxy.hpp"
#include "Geometry/CoordinateMapper.hpp"
#include "Network/Logger.hpp"
#include "OpenCV/OpenCvView.hpp"
#include "Windows/SoundPlayer.hpp"
#include "Windows/UsernamePrompt.hpp"
#include "IInputHandler.hpp"

using namespace kungfu;

namespace {

// How long the last-move highlight / start-end banner stays visible after firing.
constexpr std::chrono::milliseconds kLastMoveHighlightDuration{1000};
constexpr std::chrono::milliseconds kBannerDuration{1800};

// Tracks a transient (text, timestamp) pair and expires it after a fixed duration - the
// same shape used for both the last-move highlight and the start/end banner below.
template <typename T>
class Expiring {
public:
    void show(T value, std::chrono::milliseconds duration) {
        value_ = std::move(value);
        expiresAt_ = std::chrono::steady_clock::now() + duration;
    }

    std::optional<T> current() const {
        if (!value_.has_value() || std::chrono::steady_clock::now() > expiresAt_) {
            return std::nullopt;
        }
        return value_;
    }

private:
    std::optional<T> value_;
    std::chrono::steady_clock::time_point expiresAt_;
};

// Forwards raw pixel clicks to the server (via RemoteGameProxy) and keeps a purely
// cosmetic, optimistic echo of the select/deselect click pattern for the selection
// highlight - the server is the sole authority on whether a selection is actually legal;
// this only ever affects what's drawn, never what's sent.
class BoardClickHandler : public IInputHandler {
public:
    BoardClickHandler(RemoteGameProxy& proxy, CoordinateMapper mapper) : proxy_(proxy), mapper_(mapper) {}

    void handleClick(int x, int y) override {
        if (proxy_.isSpectator()) {
            return;  // read-only - nothing to select, nothing to send
        }

        const Position cell = mapper_.pixelToCell(x, y);
        proxy_.sendClick(cell.row(), cell.col());

        if (localSelection_.has_value()) {
            localSelection_.reset();
        } else {
            localSelection_ = cell;
        }
    }

    std::optional<int> pollEvent() override { return std::nullopt; }

    std::optional<Position> localSelection() const { return localSelection_; }

private:
    RemoteGameProxy& proxy_;
    CoordinateMapper mapper_;
    std::optional<Position> localSelection_;
};

PlayerPanel& panelFor(Scoreboard& scoreboard, PlayerColor color) {
    return color == PlayerColor::White ? scoreboard.white : scoreboard.black;
}

// UsernamePrompt stays Network-agnostic (see UI/Windows/UsernamePrompt.hpp), so this is
// the one place that maps its UI-facing outcome onto the wire protocol's JoinMode.
net::JoinMode toJoinMode(JoinInfo::Mode mode) {
    switch (mode) {
        case JoinInfo::Mode::CreateRoom: return net::JoinMode::CreateRoom;
        case JoinInfo::Mode::JoinRoom:   return net::JoinMode::JoinRoom;
        default:                        return net::JoinMode::QuickMatch;
    }
}

}  // namespace

int main(int argc, char** argv) {
    net::Logger logger("CLIENT", "logs/client.log");

    const std::string host = argc >= 2 ? argv[1] : "127.0.0.1";
    const std::string serverUrl = "ws://" + host + ":7777";

    // Username/password/mode -> LOGIN -> JOIN is a retry loop: a wrong password or a
    // quick-match search timing out both return the player to the same window (with an
    // explanatory message) rather than the app just exiting or hanging.
    std::optional<RemoteGameProxy> proxyOpt;  // constructed fresh each retry; never moved,
                                               // since its internal callbacks capture `this`
    std::string errorMessage;
    std::optional<JoinInfo> join;

    while (true) {
        join = UsernamePrompt::show(errorMessage);
        if (!join.has_value()) {
            logger.log("No username entered - exiting.");
            return 0;
        }

        logger.log("Connecting to " + serverUrl + " as \"" + join->username + "\" ...");
        proxyOpt.emplace(serverUrl, join->username, join->password, toJoinMode(join->mode), join->room);
        RemoteGameProxy& loginAttempt = *proxyOpt;

        while (!loginAttempt.loginResolved()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (loginAttempt.loginFailed()) {
            errorMessage = "Login failed: " + loginAttempt.loginFailureReason();
            logger.log(errorMessage);
            continue;
        }
        logger.log(join->username + " logged in (rating " + std::to_string(loginAttempt.myRating()) + ")");

        while (!loginAttempt.hasRole() && !loginAttempt.searchFailed()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (loginAttempt.searchFailed()) {
            errorMessage = "No opponent found within range - try again";
            logger.log(errorMessage);
            continue;
        }

        break;
    }

    RemoteGameProxy& proxy = *proxyOpt;
    if (proxy.isSpectator()) {
        logger.log("Connected as a spectator");
    } else {
        logger.log(std::string("Connected as ") + (proxy.myColor() == PlayerColor::White ? "White" : "Black"));
    }

    const int windowSize = 800;
    const int sidePanelWidth = 260;
    OpenCvView view(windowSize, sidePanelWidth);

    // The click handler shares the view's own mapper (via the read-only accessor)
    // rather than constructing a second, separate one - so the board's on-screen
    // position can never drift out of sync between drawing and click handling.
    auto clickHandler = std::make_shared<BoardClickHandler>(proxy, view.mapper());
    view.setInputHandler(clickHandler);
    view.init();

    Scoreboard scoreboard;
    scoreboard.white.name = "waiting...";
    scoreboard.black.name = "waiting...";

    SoundPlayer sounds;
    Expiring<std::string> banner;
    Expiring<std::pair<Position, Position>> lastMove;
    bool loggedForfeitWarning = false;  // edge-detect so the log line prints once, not every frame

    proxy.onMoveStarted().subscribe([&](const MoveStarted& event) {
        panelFor(scoreboard, event.color).moves.push_back({static_cast<double>(event.elapsedMs), event.notation});
        lastMove.show({event.from, event.to}, kLastMoveHighlightDuration);
        sounds.play("UI/assets/sounds/move.wav");
    });
    proxy.onPieceCaptured().subscribe([&](const PieceCaptured& event) {
        panelFor(scoreboard, event.capturingColor).score += pieceValue(event.capturedType);
        sounds.play("UI/assets/sounds/capture.wav");
    });
    proxy.onGameStarted().subscribe([&](const GameStarted&) {
        banner.show("GAME START", kBannerDuration);
    });
    proxy.onGameEnded().subscribe([&](const GameEnded& event) {
        banner.show(event.winner == PlayerColor::White ? "WHITE WINS" : "BLACK WINS", kBannerDuration);
        sounds.play("UI/assets/sounds/game_end.wav");
        logger.log(std::string("Game ended - ") + (event.winner == PlayerColor::White ? "White" : "Black") + " wins");
    });
    proxy.onForfeit().subscribe([&](const net::ForfeitMessage& event) {
        banner.show(event.winner == PlayerColor::White ? "WHITE WINS (forfeit)" : "BLACK WINS (forfeit)",
                    kBannerDuration);
        sounds.play("UI/assets/sounds/game_end.wav");
        logger.log(std::string("Opponent disconnected - ") + (event.winner == PlayerColor::White ? "White" : "Black") +
                   " wins by forfeit");
    });

    while (view.isOpen()) {
        // Publishes anything received over the network since the last frame, on this
        // (the main) thread - see RemoteGameProxy.hpp for why that matters.
        proxy.pollEvents();

        if (const auto players = proxy.players(); !players.white.empty()) {
            scoreboard.white.name = players.white;
            scoreboard.black.name = players.black;
        }
        if (const auto key = proxy.roomKey(); key.has_value()) {
            scoreboard.roomLabel = "Room: " + *key;
        }
        if (!proxy.isSpectator()) {
            panelFor(scoreboard, proxy.myColor()).rating = proxy.myRating();
        }
        if (const auto ratings = proxy.ratings(); ratings.white > 0 || ratings.black > 0) {
            scoreboard.white.rating = ratings.white;
            scoreboard.black.rating = ratings.black;
        }

        BoardHighlight highlight;
        if (const auto selected = clickHandler->localSelection(); selected.has_value()) {
            highlight.hasSelection = true;
            highlight.selectedRow = selected->row();
            highlight.selectedCol = selected->col();
        }
        if (const auto recent = lastMove.current(); recent.has_value()) {
            highlight.hasLastMove = true;
            highlight.lastMoveFromRow = recent->first.row();
            highlight.lastMoveFromCol = recent->first.col();
            highlight.lastMoveToRow = recent->second.row();
            highlight.lastMoveToCol = recent->second.col();
        }

        Banner frameBanner;
        if (const auto forfeitWarning = proxy.forfeitWarning(); forfeitWarning.has_value()) {
            if (!loggedForfeitWarning) {
                logger.log(std::string((forfeitWarning->disconnectedColor == PlayerColor::White) ? "White" : "Black") +
                           " disconnected - forfeit countdown started");
                loggedForfeitWarning = true;
            }
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                forfeitWarning->deadline - std::chrono::steady_clock::now());
            const int secondsLeft = std::max(0, static_cast<int>((remaining.count() + 999) / 1000));
            frameBanner.visible = true;
            frameBanner.text = "Opponent disconnected - auto-win in " + std::to_string(secondsLeft) + "s";
        } else if (const auto text = banner.current(); text.has_value()) {
            frameBanner.visible = true;
            frameBanner.text = *text;
        } else {
            loggedForfeitWarning = false;
        }

        view.render(proxy.getRenderState(), highlight, scoreboard, frameBanner);
    }

    return 0;
}
