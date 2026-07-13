#include "game/RealTimeArbiter.hpp"
#include <iostream>

namespace kungfu {

RealTimeArbiter::RealTimeArbiter(std::shared_ptr<IBoard> board)
    : board_(std::move(board)) {}

void RealTimeArbiter::addMove(const PendingMove& pm) {
    pendingMoves_.push_back(pm);
}

void RealTimeArbiter::advanceTime(int ms) {
    for (int t = 0; t < ms; ++t) {
        currentTimeMs_++;

        for (size_t i = 0; i < pendingMoves_.size(); ++i) {
            auto& pm = pendingMoves_[i];
            if (!pm.active) continue;

            if (currentTimeMs_ == pm.nextStepTimeMs || currentTimeMs_ == pm.arrivalTimeMs) {
                Position nextPos(pm.currentPos.row() + pm.rowStep, pm.currentPos.col() + pm.colStep);

                auto targetPieceOpt = board_->pieceAt(nextPos);
                if (targetPieceOpt.has_value()) {
                    Piece* target = targetPieceOpt.value();
                    auto currentPieceOpt = board_->pieceAt(pm.currentPos);
                    
                    if (currentPieceOpt.has_value() && target->color() == currentPieceOpt.value()->color()) {
                        currentPieceOpt.value()->setState(PieceState::Idle);
                        pm.active = false;
                        continue;
                    } else {
                        bool capturedKing = (target->type() == PieceType::King);

                        for (auto& otherPm : pendingMoves_) {
                            if (otherPm.active && otherPm.currentPos == nextPos) {
                                otherPm.active = false;
                            }
                        }

                        board_->removePiece(nextPos);
                        board_->movePiece(pm.currentPos, nextPos);
                        pm.currentPos = nextPos;

                        if (capturedKing) {
                            kingCaptured_ = true;
                            pm.active = false;
                            return;
                        }
                    }
                } else {
                    board_->movePiece(pm.currentPos, nextPos);
                    pm.currentPos = nextPos;
                }

                if (pm.currentPos == pm.to || currentTimeMs_ >= pm.arrivalTimeMs) {
                    pm.active = false;
                    if (auto movedPiece = board_->pieceAt(pm.to); movedPiece.has_value()) {
                        movedPiece.value()->setState(PieceState::Idle);
                        // חוק 5: לוגיקת ה-Promotion הוסרה מכאן לחלוטין
                    }
                } else {
                    pm.nextStepTimeMs += 1000;
                }
            }
        }

        pendingMoves_.erase(
            std::remove_if(pendingMoves_.begin(), pendingMoves_.end(),
                           [](const PendingMove& pm) { return !pm.active; }),
            pendingMoves_.end());

        if (kingCaptured_) return;
    }
}

}  // namespace kungfu