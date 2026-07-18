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

// One piece's in-flight move, tracked cell-by-cell. Owned exclusively by
// RealTimeArbiter; GameEngine only ever reads a snapshot of these (via
// snapshotPendingMoves) to interpolate a piece's on-screen position.
struct PendingMove {
    Position from;           // requested starting square
    Position to;              // requested destination square
    Position currentPos;      // the square the piece is actually on right now
    int startTimeMs;          // simulated time the move began
    int arrivalTimeMs;        // simulated time it will reach 'to', barring a collision
    int nextStepTimeMs;       // simulated time of the next single-cell step
    int rowStep;               // -1, 0, or 1: row direction of travel
    int colStep;               // -1, 0, or 1: column direction of travel
    bool active = true;        // false once the move has ended (arrival, capture, or block)
    uintptr_t pieceId = 0;     // non-owning identifier for which piece this move belongs to
};

// One piece being removed from the board as a genuine capture (as opposed
// to just repositioning, or a friendly block) - who gets credit and what
// was eliminated. Recorded during advanceTime; GameEngine drains these
// after each wait() to update score/history, so RealTimeArbiter itself
// never needs to know anything about scoring.
struct CaptureEvent {
    PlayerColor capturingColor;
    PieceType capturedType;
};

class RealTimeArbiter {
public:
    // speedMultiplier scales both movement speed and post-move cooldown
    // (>1.0 = faster game, <1.0 = slower). Defaults to 1.0, matching
    // GameConfig's fixed kCellSizePx/kPieceSpeedPxPerSec constants exactly.
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
    // PieceState::Airborne. Once the timer expires it lands into a short
    // rest (see resolveAirborneExpirations), same as after a real move's
    // arrival; if an enemy's real-time move reaches its square first,
    // advanceTime resolves the counter-kill instead (the jumper survives
    // and lands, the attacker is the one removed).
    void beginAirborne(uintptr_t pieceId);

    // Return a snapshot copy of pending moves for external inspection
    std::vector<PendingMove> snapshotPendingMoves() const;

    int currentTimeMs() const { return currentTimeMs_; }
    bool isKingCaptured() const { return kingCaptured_; }
    bool hasActiveMoves() const { return !pendingMoves_.empty(); }
    int msPerCell() const { return msPerCell_; }
    int cooldownMs() const { return cooldownMs_; }
    int airborneMs() const { return airborneMs_; }
    int shortRestMs() const { return shortRestMs_; }

    // Remaining post-move cooldown for one piece, in ms; 0 if it isn't
    // currently on cooldown. Lets GameEngine::getRenderState surface real
    // cooldown progress to the UI without exposing the timer bookkeeping
    // this class keeps privately.
    int cooldownRemainingMs(uintptr_t pieceId) const;

    // Same as cooldownRemainingMs, for the short rest a piece enters after
    // landing from a jump (see PieceState::ShortRest).
    int shortRestRemainingMs(uintptr_t pieceId) const;

    // Returns every capture recorded since the last call, then clears the
    // list - a one-shot drain, meant to be called once per GameEngine::wait().
    std::vector<CaptureEvent> drainCaptureEvents();

private:
    // A piece-id + expiry-time pair, shared by every "this piece is
    // temporarily unavailable, and may have a premove queued" timer this
    // class tracks (cooldown, airborne, short rest).
    struct TimerEntry {
        uintptr_t pieceId;
        int endTimeMs;
    };

    void promoteIfNeeded(const Position& pos);

    // Cooldown (after a real move's arrival) and short rest (after landing
    // from a jump) are structurally the same "piece temporarily
    // unavailable, may have a premove queued" timer - just a different
    // duration and trigger point. Both are thin wrappers around the shared
    // beginRest/restRemainingMs/resolveRestExpirations machinery below, so
    // call sites keep purpose-specific names.
    void beginCooldown(Piece* piece);
    void beginShortRest(Piece* piece);
    void resolveCooldownExpirations();
    void resolveShortRestExpirations();

    void resolveAirborneExpirations();
    // An enemy's real-time move stepped onto a square held by an airborne
    // piece: the jumper is immune, so it lands into a short rest and the
    // attacker is removed instead of the usual capture-and-advance.
    void resolveAirborneCounterKill(PendingMove& pm, Piece* attacker, Piece* airbornePiece);
    void recordCapture(PlayerColor capturingColor, PieceType capturedType);

    Piece* findPieceById(uintptr_t pieceId) const;
    void beginRest(Piece* piece, PieceState state, int durationMs, std::vector<TimerEntry>& entries);
    int restRemainingMs(uintptr_t pieceId, const std::vector<TimerEntry>& entries) const;
    // Lands every entry in 'entries' whose timer just expired back to Idle
    // (skipping any that already changed state some other way, e.g. a
    // counter-kill), then fires its queued premove, if any and still legal.
    void resolveRestExpirations(std::vector<TimerEntry>& entries, PieceState expectedState);

    // One tick's worth of resolution for a single in-flight move, split by
    // movement style (see advanceTime). Both return true only when this
    // resolution captured a king - advanceTime stops immediately when that
    // happens, matching real-time play with no further moves after game over.
    bool resolveKnightArrival(PendingMove& pm, Piece* currentPiece);
    bool resolveStepMove(PendingMove& pm, Piece* currentPiece);

    std::shared_ptr<IBoard> board_;
    std::shared_ptr<IRuleEngine> ruleEngine_;
    int msPerCell_;
    int cooldownMs_;
    int airborneMs_;
    int shortRestMs_;
    std::vector<PendingMove> pendingMoves_;
    std::vector<TimerEntry> cooldowns_;
    std::vector<TimerEntry> airborneEntries_;
    std::vector<TimerEntry> shortRestEntries_;
    std::unordered_map<uintptr_t, Position> premoves_;
    std::vector<CaptureEvent> captureEvents_;
    int currentTimeMs_ = 0;
    bool kingCaptured_ = false;
};

}  // namespace kungfu
