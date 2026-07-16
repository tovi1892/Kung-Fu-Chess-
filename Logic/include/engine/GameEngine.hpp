#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "model/IBoard.hpp"
#include "model/Enums.hpp"
#include "model/Position.hpp"
#include "movement/MovementSystem.hpp"
#include "model/pieces/Piece.hpp"
#include "rules/IRuleEngine.hpp"
#include "realtime/RealTimeArbiter.hpp"
#include "history/GameRecord.hpp"

// Forward-declare render struct from Core_Interfaces to avoid direct header dependency
namespace kungfu { struct RenderPiece; }

namespace kungfu {

// Result of a requestMove() call. reason is always present: "ok" for an
// accepted move, "game_over" when the game has already ended, or a
// rule-level reason copied from RuleEngine's MoveValidation.
struct MoveResult {
    bool is_accepted;
    std::string reason;
};

// GameEngine: application-service coordinator. Owns game-over/state, delegates
// legality to IRuleEngine, and starts/advances motions through RealTimeArbiter.
// It does not own click interpretation or selection state (that's Controller's
// job) and does not own piece-specific movement logic (that's PieceRules', via
// RuleEngine).
class GameEngine {
public:
    // A fresh, empty 8x8 board with a default RuleEngine.
    GameEngine();

    // An existing board, with a default RuleEngine.
    explicit GameEngine(std::shared_ptr<IBoard> board);

    // An existing board and rule engine. speedMultiplier scales both
    // movement speed and post-move cooldown/airborne duration in
    // RealTimeArbiter (>1.0 = faster game). Defaults to 1.0.
    GameEngine(std::shared_ptr<IBoard> board, std::shared_ptr<IRuleEngine> ruleEngine,
               double speedMultiplier = 1.0);

    // Marks the game as actively running. wait()/requestMove() have no
    // effect until this is called.
    void start();

    // Pauses the game; wait() becomes a no-op until start() is called again.
    void stop();

    // True while the game is actively running (after start(), before
    // stop() or a king capture).
    bool isRunning() const;

    // True once a king has been captured and the game has ended.
    bool isFinished() const;

    // Advances the simulated game clock by 'ms' milliseconds, resolving any
    // in-flight moves, cooldowns, and airborne timers along the way. A
    // no-op unless the game is currently running.
    void wait(int ms);

    // Requests a move for whichever piece is at 'from', toward 'to'. See
    // MoveResult above for the possible outcomes and their reasons.
    MoveResult requestMove(const Position& from, const Position& to);

    // Requests a jump for the piece at 'pos' - a thin wrapper over tryJump
    // below, and the entry point Controller actually calls.
    bool requestJump(const Position& pos);

    // Experimental "jump" mechanic (PieceState::Airborne) — not part of the
    // graded common/extra route. tryJump is the real entry point used during
    // play (Controller re-clicking the same cell): it starts an airborne
    // timer in RealTimeArbiter (GameConfig::kBaseAirborneMs, scaled by
    // speedMultiplier like cooldown) that lands the piece into a short rest
    // (PieceState::ShortRest, GameConfig::kBaseShortRestMs) on its own, and
    // RealTimeArbiter::advanceTime resolves the counter-kill in real time if
    // an enemy's move reaches the airborne square first (the jumper
    // survives and lands into the same short rest, the attacker is removed
    // instead of the usual capture). resolveJump/handleArrivalAtAirbornCell
    // below remain as direct manual hooks (used by unit tests, and forcing
    // straight to Idle rather than through a short rest) but are no longer
    // the path real gameplay takes to land a jump or resolve a landing
    // collision.
    bool tryJump(const Position& cell);

    // Manual hook: forces an airborne piece back to Idle (skipping the short
    // rest real landings go through). Not used by live gameplay (see
    // tryJump above) - kept for direct unit testing.
    void resolveJump(const Position& cell);

    // Manual hook mirroring RealTimeArbiter's real-time counter-kill: if the
    // piece at 'cell' is airborne and 'arrivingFrom' holds an enemy piece,
    // the attacker is removed and the jumper lands. Not used by live
    // gameplay - kept for direct unit testing.
    bool handleArrivalAtAirbornCell(const Position& cell, const Position& arrivingFrom);

    // The board this engine is playing on.
    std::shared_ptr<IBoard> getBoard() const;

    // A snapshot of every piece's renderable state for the UI (non-owning,
    // lightweight) - see RenderPiece in Core_Interfaces/IGameView.hpp.
    std::vector<kungfu::RenderPiece> getRenderState() const;

    // True when 'pos' falls within the board's actual dimensions.
    bool isPositionInBounds(const Position& pos) const;

    // The board's row/column count (8x8 unless constructed otherwise, e.g.
    // by BoardParser from a script with a different shape).
    int boardRows() const;
    int boardCols() const;

    // The running move history and score for both colors, built up as moves
    // are accepted and captures resolve. See history/GameRecord.hpp.
    const GameRecord& gameRecord() const { return record_; }

private:
    GameState state_;
    std::shared_ptr<IBoard> board_;
    std::shared_ptr<IRuleEngine> ruleEngine_;
    MovementSystem movementSystem_;
    GameRecord record_;

    // All real-time move/cooldown/airborne timing is delegated to the Arbiter.
    std::unique_ptr<RealTimeArbiter> arbiter_;
};

}  // namespace kungfu
