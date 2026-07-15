#include "realtime/RealTimeArbiter.hpp"
#include <iostream>
#include <algorithm>
#include "model/GameConfig.hpp"
#include "model/pieces/Queen.hpp"

namespace kungfu {

RealTimeArbiter::RealTimeArbiter(std::shared_ptr<IBoard> board)
    : board_(std::move(board)), currentTimeMs_(0), kingCaptured_(false) {}

void RealTimeArbiter::promoteIfNeeded(const Position& pos) {
    const auto pieceOpt = board_->pieceAt(pos);
    if (!pieceOpt.has_value() || pieceOpt.value() == nullptr ||
        pieceOpt.value()->type() != PieceType::Pawn) {
        return;
    }
    const PlayerColor color = pieceOpt.value()->color();
    const int promotionRow = (color == PlayerColor::White)
                                  ? GameConfig::kWhitePawnPromotionRow
                                  : GameConfig::kBlackPawnPromotionRow;
    if (pos.row() == promotionRow) {
        board_->replacePiece(pos, std::make_unique<Queen>(color, pos));
    }
}

void RealTimeArbiter::addMove(const PendingMove& pm) {
    pendingMoves_.push_back(pm);
}

void RealTimeArbiter::advanceTime(int ms) {
    for (int t = 0; t < ms; ++t) {
        currentTimeMs_++;

        for (size_t i = 0; i < pendingMoves_.size(); ++i) {
            auto& pm = pendingMoves_[i];
            if (!pm.active) continue;

            auto currentPieceOpt = board_->pieceAt(pm.currentPos);
            if (!currentPieceOpt.has_value() || currentPieceOpt.value() == nullptr) {
                pm.active = false;
                continue;
            }
            Piece* currentPiece = currentPieceOpt.value();

            // Knights jump in one L-shaped hop: there is no intermediate square
            // to block or be captured along the way, so nothing happens until
            // arrival, and only the landing square (pm.to) is ever inspected -
            // never whatever the knight jumped over.
            if (currentPiece->type() == PieceType::Knight) {
                if (currentTimeMs_ != pm.arrivalTimeMs) {
                    continue;
                }
                pm.active = false;

                const auto targetPieceOpt = board_->pieceAt(pm.to);
                if (targetPieceOpt.has_value() && targetPieceOpt.value() != nullptr) {
                    Piece* target = targetPieceOpt.value();
                    if (target->color() == currentPiece->color()) {
                        // Can never land on a friendly-occupied square - the jump aborts in place.
                        currentPiece->setState(PieceState::Idle);
                        continue;
                    }

                    const bool capturedKing = (target->type() == PieceType::King);
                    board_->removePiece(pm.to);
                    board_->movePiece(pm.currentPos, pm.to);
                    pm.currentPos = pm.to;
                    currentPiece->setState(PieceState::Idle);
                    if (capturedKing) {
                        kingCaptured_ = true;
                        return;
                    }
                    continue;
                }

                board_->movePiece(pm.currentPos, pm.to);
                pm.currentPos = pm.to;
                currentPiece->setState(PieceState::Idle);
                continue;
            }

            if (currentTimeMs_ == pm.nextStepTimeMs || currentTimeMs_ == pm.arrivalTimeMs) {
                Position nextPos(pm.currentPos.row() + pm.rowStep, pm.currentPos.col() + pm.colStep);

                auto targetPieceOpt = board_->pieceAt(nextPos);
                if (targetPieceOpt.has_value() && targetPieceOpt.value() != nullptr) {
                    Piece* target = targetPieceOpt.value();

                    // בדיקה האם כלי היעד נמצא כרגע בתנועה פעילה משלו
                    PendingMove* otherPm = nullptr;
                    for (auto& opm : pendingMoves_) {
                        if (opm.active && opm.currentPos == nextPos) {
                            otherPm = &opm;
                            break;
                        }
                    }

                    if (otherPm != nullptr) {
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
                        // הכלי ביעד סטטי (אינו זז)
                        if (target->color() == currentPiece->color()) {
                            // חסימה סטטית ידידותית
                            currentPiece->setState(PieceState::Idle);
                            pm.active = false;
                            continue;
                        } else {
                            // הכאה רגילה של אויב סטטי
                            bool capturedKing = (target->type() == PieceType::King);

                            // עצירת תנועות אחרות שרצו אל אותה משבצת היעד שנתפסה כעת
                            for (auto& otherPm2 : pendingMoves_) {
                                if (otherPm2.active && otherPm2.currentPos == nextPos) {
                                    otherPm2.active = false;
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
                    promoteIfNeeded(pm.to);
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