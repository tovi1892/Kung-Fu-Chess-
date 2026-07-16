#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "model/IBoard.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"
#include "rules/IRuleEngine.hpp"

namespace kungfu {

// המבנה הועבר לכאן - שייך בלעדית לשכבת ה-Arbiter
struct PendingMove {
    Position from;
    Position to;
    Position currentPos;
    int startTimeMs;
    int arrivalTimeMs;
    int nextStepTimeMs;
    int rowStep;
    int colStep;
    bool active = true;
    // Non-owning identifier for which piece this pending move belongs to
    uintptr_t pieceId = 0;
};

class RealTimeArbiter {
public:
    // speedMultiplier scales both movement speed and post-move cooldown
    // (>1.0 = faster game, <1.0 = slower). Defaults to 1.0, matching the
    // course spec's fixed CELL_SIZE/PIECE_SPEED constants exactly.
    RealTimeArbiter(std::shared_ptr<IBoard> board,
                     std::shared_ptr<IRuleEngine> ruleEngine,
                     double speedMultiplier = 1.0);

    void advanceTime(int ms);

    // Builds and starts a PendingMove for the given piece. Single source of
    // truth for the distance/step/timing math, used both by GameEngine and
    // internally when a queued premove becomes eligible to run.
    void startMove(const Position& from, const Position& to, uintptr_t pieceId);

    // Queues (or overwrites) a premove for a piece currently on cooldown.
    // Intentionally unvalidated at queue time: submitting any new request for
    // the same piece - legal or not - replaces whatever was queued, which is
    // exactly how a deliberately-illegal request cancels a premove.
    void setPremove(uintptr_t pieceId, const Position& to);

    // Starts the airborne timer for a piece GameEngine::tryJump just put into
    // PieceState::Airborne. Once the timer expires it lands back to Idle on
    // its own (see resolveAirborneExpirations); if an enemy's real-time move
    // reaches its square first, advanceTime resolves the counter-kill instead
    // (the jumper survives and lands, the attacker is the one removed).
    void beginAirborne(uintptr_t pieceId);

    // Return a snapshot copy of pending moves for external inspection
    std::vector<PendingMove> snapshotPendingMoves() const;

    int currentTimeMs() const { return currentTimeMs_; }
    bool isKingCaptured() const { return kingCaptured_; }
    bool hasActiveMoves() const { return !pendingMoves_.empty(); }
    int msPerCell() const { return msPerCell_; }
    int cooldownMs() const { return cooldownMs_; }
    int airborneMs() const { return airborneMs_; }

    // Remaining post-move cooldown for one piece, in ms; 0 if it isn't
    // currently on cooldown. Lets GameEngine::getRenderState surface real
    // cooldown progress to the UI without exposing the CooldownEntry
    // bookkeeping this class keeps privately.
    int cooldownRemainingMs(uintptr_t pieceId) const;

private:
    struct CooldownEntry {
        uintptr_t pieceId;
        int endTimeMs;
    };

    struct AirborneEntry {
        uintptr_t pieceId;
        int endTimeMs;
    };

    void promoteIfNeeded(const Position& pos);
    void beginCooldown(Piece* piece);
    void resolveCooldownExpirations();
    void resolveAirborneExpirations();
    // An enemy's real-time move stepped onto a square held by an airborne
    // piece: the jumper is immune, so it lands (back to Idle) and the
    // attacker is removed instead of the usual capture-and-advance.
    void resolveAirborneCounterKill(PendingMove& pm, Piece* attacker, Piece* airbornePiece);

    std::shared_ptr<IBoard> board_;
    std::shared_ptr<IRuleEngine> ruleEngine_;
    int msPerCell_;
    int cooldownMs_;
    int airborneMs_;
    std::vector<PendingMove> pendingMoves_;
    std::vector<CooldownEntry> cooldowns_;
    std::vector<AirborneEntry> airborneEntries_;
    std::unordered_map<uintptr_t, Position> premoves_;
    int currentTimeMs_ = 0;
    bool kingCaptured_ = false;
};

}  // namespace kungfu
