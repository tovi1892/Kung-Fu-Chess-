#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "history/GameRecord.hpp"
#include "model/Enums.hpp"
#include "model/Position.hpp"

#include "Geometry/CoordinateMapper.hpp"
#include "NetClient/RemoteGameProxy.hpp"
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

}  // namespace

int main(int argc, char** argv) {
    const auto username = UsernamePrompt::show();
    if (!username.has_value()) {
        std::cout << "No username entered - exiting." << std::endl;
        return 0;
    }

    const std::string host = argc >= 2 ? argv[1] : "127.0.0.1";
    const std::string serverUrl = "ws://" + host + ":7777";

    RemoteGameProxy proxy(serverUrl, *username);

    std::cout << "Connecting to " << serverUrl << " as \"" << *username << "\" ..." << std::endl;
    while (!proxy.hasColor()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << "Connected as " << (proxy.myColor() == PlayerColor::White ? "White" : "Black") << std::endl;

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
    });

    while (view.isOpen()) {
        // Publishes anything received over the network since the last frame, on this
        // (the main) thread - see RemoteGameProxy.hpp for why that matters.
        proxy.pollEvents();

        if (const auto players = proxy.players(); !players.white.empty()) {
            scoreboard.white.name = players.white;
            scoreboard.black.name = players.black;
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
        if (const auto text = banner.current(); text.has_value()) {
            frameBanner.visible = true;
            frameBanner.text = *text;
        }

        view.render(proxy.getRenderState(), highlight, scoreboard, frameBanner);
    }

    return 0;
}
