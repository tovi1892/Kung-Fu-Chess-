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

    // Return a snapshot copy of pending moves for external inspection
    std::vector<PendingMove> snapshotPendingMoves() const;

    int currentTimeMs() const { return currentTimeMs_; }
    bool isKingCaptured() const { return kingCaptured_; }
    bool hasActiveMoves() const { return !pendingMoves_.empty(); }
    int msPerCell() const { return msPerCell_; }
    int cooldownMs() const { return cooldownMs_; }

private:
    struct CooldownEntry {
        uintptr_t pieceId;
        int endTimeMs;
    };

    void promoteIfNeeded(const Position& pos);
    void beginCooldown(Piece* piece);
    void resolveCooldownExpirations();

    std::shared_ptr<IBoard> board_;
    std::shared_ptr<IRuleEngine> ruleEngine_;
    int msPerCell_;
    int cooldownMs_;
    std::vector<PendingMove> pendingMoves_;
    std::vector<CooldownEntry> cooldowns_;
    std::unordered_map<uintptr_t, Position> premoves_;
    int currentTimeMs_ = 0;
    bool kingCaptured_ = false;
};

}  // namespace kungfu
