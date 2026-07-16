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

// Forward-declare render struct from Core_Interfaces to avoid direct header dependency
namespace kungfu { struct RenderPiece; }

namespace kungfu {

// Application-service result for a move request 
// reason is always present: "ok" for an accepted move, "game_over" when the
// game has already ended, or a rule-level reason copied from MoveValidation.
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
    GameEngine();
    explicit GameEngine(std::shared_ptr<IBoard> board);
    // speedMultiplier scales both movement speed and post-move cooldown in
    // RealTimeArbiter (>1.0 = faster game). Defaults to 1.0.
    GameEngine(std::shared_ptr<IBoard> board, std::shared_ptr<IRuleEngine> ruleEngine,
               double speedMultiplier = 1.0);

    void start();
    void stop();
    bool isRunning() const;
    bool isFinished() const;

    void wait(int ms);

    MoveResult requestMove(const Position& from, const Position& to);
    bool requestJump(const Position& pos);

    // Experimental "jump" mechanic (PieceState::Airborne) — not part of the
    // graded common/extra route. tryJump is the real entry point used during
    // play (Controller re-clicking the same cell): it starts an airborne
    // timer in RealTimeArbiter (GameConfig::kBaseAirborneMs, scaled by
    // speedMultiplier like cooldown) that lands the piece back to Idle on its
    // own, and RealTimeArbiter::advanceTime resolves the counter-kill in
    // real time if an enemy's move reaches the airborne square first (the
    // jumper survives, the attacker is removed instead of the usual
    // capture). resolveJump/handleArrivalAtAirbornCell below remain as
    // direct manual hooks (used by unit tests) but are no longer the path
    // real gameplay takes to land a jump or resolve a landing collision.
    bool tryJump(const Position& cell);
    void resolveJump(const Position& cell);
    bool handleArrivalAtAirbornCell(const Position& cell, const Position& arrivingFrom);

    std::shared_ptr<IBoard> getBoard() const;

    // Snapshot of renderable state for the UI (non-owning, lightweight)
    std::vector<kungfu::RenderPiece> getRenderState() const;

    bool isPositionInBounds(const Position& pos) const;
    int boardRows() const;
    int boardCols() const;

private:
    GameState state_;
    std::shared_ptr<IBoard> board_;
    std::shared_ptr<IRuleEngine> ruleEngine_;
    MovementSystem movementSystem_;

    // הפרדת האחריות: ניהול הזמן והתנועות עבר ל-Arbiter
    std::unique_ptr<RealTimeArbiter> arbiter_;
};

}  // namespace kungfu
