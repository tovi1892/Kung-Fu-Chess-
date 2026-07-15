#include "game/RealTimeArbiter.hpp"
#include <iostream>
#include <algorithm>
#include "model/GameConfig.hpp"

namespace kungfu {

RealTimeArbiter::RealTimeArbiter(std::shared_ptr<IBoard> board)
    : board_(std::move(board)), currentTimeMs_(0), kingCaptured_(false) {}

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

                auto currentPieceOpt = board_->pieceAt(pm.currentPos);
                if (!currentPieceOpt.has_value() || currentPieceOpt.value() == nullptr) {
                    pm.active = false;
                    continue;
                }
                Piece* currentPiece = currentPieceOpt.value();

                auto targetPieceOpt = board_->pieceAt(nextPos);
                if (targetPieceOpt.has_value() && targetPieceOpt.value() != nullptr) {
                    Piece* target = targetPieceOpt.value();

                    // חוק 8: פרש נוחת ביעד הסופי שלו (מחסל כל כלי, גם ידידותי)
                    if (currentPiece->type() == PieceType::Knight && nextPos == pm.to) {
                        bool capturedKing = (target->type() == PieceType::King);
                        board_->removePiece(nextPos);
                        board_->movePiece(pm.currentPos, nextPos);
                        pm.currentPos = nextPos;

                        if (capturedKing) {
                            kingCaptured_ = true;
                            pm.active = false;
                            return;
                        }
                    }
                    else {
                        // בדיקה האם כלי היעד נמצא כרגע בתנועה פעילה משלו
                        PendingMove* otherPm = nullptr;
                        for (auto& opm : pendingMoves_) {
                            if (opm.active && opm.currentPos == nextPos) {
                                otherPm = &opm;
                                break;
                            }
                        }

                        // חוק 5: פרשים מוחרגים מהתנגשויות במסלול השיוט שלהם
                        bool isKnightInvolved = (currentPiece->type() == PieceType::Knight || target->type() == PieceType::Knight);

                        if (otherPm != nullptr && !isKnightInvolved) {
                            // התנגשות דינמית בזמן תנועה של שני כלים (Active Collision)
                            if (target->color() == currentPiece->color()) {
                                // חוק 6: התנגשות ידידים - הכלי שמנסה להיכנס (pm) נעצר במקומו מיד
                                currentPiece->setState(PieceState::Idle);
                                pm.active = false;
                                continue; 
                            } else {
                                // חוק 5: התנגשות אויבים - זה שהתחיל לזוז קודם (startTimeMs נמוך יותר) מנצח!
                                if (pm.startTimeMs < otherPm->startTimeMs) {
                                    // pm מנצח ומחסל את otherPm
                                    target->setState(PieceState::Captured);
                                    board_->removePiece(nextPos);
                                    otherPm->active = false;

                                    board_->movePiece(pm.currentPos, nextPos);
                                    pm.currentPos = nextPos;
                                } else {
                                    // otherPm מנצח ומחסל את pm
                                    currentPiece->setState(PieceState::Captured);
                                    board_->removePiece(pm.currentPos);
                                    pm.active = false;
                                    continue;
                                }
                            }
                        } else {
                            // הכלי ביעד סטטי (אינו זז) או שאחד מהם הוא פרש המדלג מעליו
                            if (target->color() == currentPiece->color()) {
                                // חסימה סטטית ידידותית (עבור כלים שאינם פרש)
                                currentPiece->setState(PieceState::Idle);
                                pm.active = false;
                                continue;
                            } else {
                                // הכאה רגילה של אויב סטטי
                                bool capturedKing = (target->type() == PieceType::King);

                                // עצירת תנועות אחרות שרצו אל אותה משבצת היעד שנתפסה כעת
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
                        }
                    }
                } else {
                    // אין שום כלי במשבצת הבאה - התקדמות רגילה במסלול
                    board_->movePiece(pm.currentPos, nextPos);
                    pm.currentPos = nextPos;
                }

                // בדיקה האם הגענו ליעד הסופי של התנועה
                if (pm.currentPos == pm.to || currentTimeMs_ >= pm.arrivalTimeMs) {
                    pm.active = false;
                    if (auto movedPiece = board_->pieceAt(pm.to); movedPiece.has_value()) {
                        movedPiece.value()->setState(PieceState::Idle);
                    }
                } else {
                    pm.nextStepTimeMs += GameConfig::kMsPerCell;
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

std::vector<PendingMove> RealTimeArbiter::snapshotPendingMoves() const {
    return pendingMoves_;
}

}