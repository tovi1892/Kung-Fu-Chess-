#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>

#include "io/BoardParser.hpp"
#include "model/Board.hpp"
#include "engine/GameEngine.hpp"
#include "input/Controller.hpp"
#include "rules/RuleEngine.hpp"
#include "events/GameEvents.hpp"

#include "Core/CoordinateMapper.hpp"
#include "UI_OpenCV/OpenCvView.hpp"
#include "UI_OpenCV/SoundPlayer.hpp"
#include "IInputHandler.hpp"

using namespace kungfu;

namespace {

// Row order follows GameConfig: White starts at row 1 and advances toward row 7
// (kWhitePawnStartRow/kWhitePawnPromotionRow), Black starts at row 6 and advances toward row 0.
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

// How long the last-move highlight stays visible after a move/jump fires.
// Purely cosmetic, so it lives here rather than in Controller (which only
// owns selection state) or GameEngine (which has no concept of "recent").
constexpr std::chrono::milliseconds kLastMoveHighlightDuration{1000};

// How long a start/end banner stays on screen after firing.
constexpr std::chrono::milliseconds kBannerDuration{1800};

// Tracks the most recent game-lifecycle banner (if any) that's still fresh enough to
// show - same "remember it, expire it after a fixed duration" shape as
// BoardClickHandler::recentMove() below, just not tied to clicks.
class BannerTracker {
public:
    void show(std::string text) {
        text_ = std::move(text);
        shownAt_ = std::chrono::steady_clock::now();
    }

    Banner current() const {
        Banner banner;
        if (!text_.has_value()) {
            return banner;
        }
        if (std::chrono::steady_clock::now() - shownAt_ > kBannerDuration) {
            return banner;
        }
        banner.visible = true;
        banner.text = *text_;
        return banner;
    }

private:
    std::optional<std::string> text_;
    std::chrono::steady_clock::time_point shownAt_;
};

// Translates raw pixel clicks from the view into board-cell clicks on the
// controller, and separately remembers the most recent accepted move/jump
// purely so main()'s render loop can show a brief "these are the squares
// that just moved" highlight - a presentation concern with no effect on
// gameplay, so it's tracked here instead of inside Controller or GameEngine.
class BoardClickHandler : public IInputHandler {
public:
    BoardClickHandler(Controller& controller, CoordinateMapper mapper)
        : controller_(controller), mapper_(mapper) {}

    void handleClick(int x, int y) override {
        const Position cell = mapper_.pixelToCell(x, y);
        const auto from = controller_.selectedPosition();

        const bool accepted = controller_.handleCellClick(cell.row(), cell.col());

        if (accepted && from.has_value()) {
            lastMoveFrom_ = *from;
            lastMoveTo_ = cell;
            lastMoveAt_ = std::chrono::steady_clock::now();
        }
    }

    std::optional<int> pollEvent() override { return std::nullopt; }

    // Only returns a value while the most recent move/jump is still fresh
    // (see kLastMoveHighlightDuration) - main() treats "no value" as
    // "nothing to highlight".
    std::optional<std::pair<Position, Position>> recentMove() const {
        if (!lastMoveFrom_.has_value()) {
            return std::nullopt;
        }
        const auto elapsed = std::chrono::steady_clock::now() - lastMoveAt_;
        if (elapsed > kLastMoveHighlightDuration) {
            return std::nullopt;
        }
        return std::make_pair(*lastMoveFrom_, *lastMoveTo_);
    }

private:
    Controller& controller_;
    CoordinateMapper mapper_;
    std::optional<Position> lastMoveFrom_;
    std::optional<Position> lastMoveTo_;
    std::chrono::steady_clock::time_point lastMoveAt_;
};

PlayerPanel& panelFor(Scoreboard& scoreboard, PlayerColor color) {
    return color == PlayerColor::White ? scoreboard.white : scoreboard.black;
}

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

    auto controller = std::make_shared<Controller>(game);

    const int windowSize = 800;
    const int sidePanelWidth = 260;
    OpenCvView view(windowSize, sidePanelWidth);

    // The click handler shares the view's own mapper (via the read-only
    // accessor) rather than constructing a second, separate one - so the
    // board's on-screen position (now offset by the left side panel) can
    // never drift out of sync between drawing and click handling.
    auto clickHandler = std::make_shared<BoardClickHandler>(*controller, view.mapper());
    view.setInputHandler(clickHandler);
    view.init();

    // The side panels are now built once, here, and kept up to date by the
    // subscribers below instead of being rebuilt from GameRecord every
    // frame - the same shape a future networked client will use once it's
    // building its panels from events received over the wire instead of a
    // local GameEngine's bus.
    Scoreboard scoreboard;
    scoreboard.white.name = "white";
    scoreboard.black.name = "black";

    SoundPlayer sounds;
    BannerTracker banner;

    game->onMoveStarted().subscribe([&](const MoveStarted& event) {
        panelFor(scoreboard, event.color).moves.push_back({static_cast<double>(event.elapsedMs), event.notation});
        sounds.play("UI/assets/sounds/move.wav");
    });
    game->onPieceCaptured().subscribe([&](const PieceCaptured& event) {
        panelFor(scoreboard, event.capturingColor).score += pieceValue(event.capturedType);
        sounds.play("UI/assets/sounds/capture.wav");
    });
    game->onGameStarted().subscribe([&](const GameStarted&) {
        banner.show("GAME START");
    });
    game->onGameEnded().subscribe([&](const GameEnded& event) {
        banner.show(event.winner == PlayerColor::White ? "WHITE WINS" : "BLACK WINS");
        sounds.play("UI/assets/sounds/game_end.wav");
    });

    game->start();

    while (view.isOpen()) {
        controller->handleTimePassed(16);

        BoardHighlight highlight;
        if (const auto selected = controller->selectedPosition(); selected.has_value()) {
            highlight.hasSelection = true;
            highlight.selectedRow = selected->row();
            highlight.selectedCol = selected->col();
        }
        if (const auto recent = clickHandler->recentMove(); recent.has_value()) {
            highlight.hasLastMove = true;
            highlight.lastMoveFromRow = recent->first.row();
            highlight.lastMoveFromCol = recent->first.col();
            highlight.lastMoveToRow = recent->second.row();
            highlight.lastMoveToCol = recent->second.col();
        }

        view.render(game->getRenderState(), highlight, scoreboard, banner.current());
    }

    return 0;
}
