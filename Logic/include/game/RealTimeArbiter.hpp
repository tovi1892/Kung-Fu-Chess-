#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include "model/IBoard.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

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
    explicit RealTimeArbiter(std::shared_ptr<IBoard> board);

    void advanceTime(int ms);
    void addMove(const PendingMove& pm);
    // Return a snapshot copy of pending moves for external inspection
    std::vector<PendingMove> snapshotPendingMoves() const;
    
    int currentTimeMs() const { return currentTimeMs_; }
    bool isKingCaptured() const { return kingCaptured_; }
    bool hasActiveMoves() const { return !pendingMoves_.empty(); }

private:
    std::shared_ptr<IBoard> board_;
    std::vector<PendingMove> pendingMoves_;
    int currentTimeMs_ = 0;
    bool kingCaptured_ = false;
};

}  // namespace kungfu