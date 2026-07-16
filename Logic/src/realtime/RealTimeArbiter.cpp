#include "realtime/RealTimeArbiter.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include "model/GameConfig.hpp"
#include "model/pieces/Queen.hpp"

namespace kungfu {

RealTimeArbiter::RealTimeArbiter(std::shared_ptr<IBoard> board,
                                  std::shared_ptr<IRuleEngine> ruleEngine,
                                  double speedMultiplier)
    : board_(std::move(board)),
      ruleEngine_(std::move(ruleEngine)),
      msPerCell_(static_cast<int>(GameConfig::kMsPerCell / speedMultiplier)),
      cooldownMs_(static_cast<int>(GameConfig::kBaseCooldownMs / speedMultiplier)),
      airborneMs_(static_cast<int>(GameConfig::kBaseAirborneMs / speedMultiplier)),
      currentTimeMs_(0),
      kingCaptured_(false) {}

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

void RealTimeArbiter::beginCooldown(Piece* piece) {
    if (!piece) return;
    piece->setState(PieceState::Cooldown);
    cooldowns_.push_back({static_cast<uintptr_t>(piece->id()), currentTimeMs_ + cooldownMs_});
}

void RealTimeArbiter::startMove(const Position& from, const Position& to, uintptr_t pieceId) {
    const int rowDelta = to.row() - from.row();
    const int colDelta = to.col() - from.col();
    const int distance = std::max(std::abs(rowDelta), std::abs(colDelta));
    const int rStep = (rowDelta == 0) ? 0 : (rowDelta > 0 ? 1 : -1);
    const int cStep = (colDelta == 0) ? 0 : (colDelta > 0 ? 1 : -1);

    const int startTime = currentTimeMs_;
    PendingMove pm{
        from,
        to,
        from,
        startTime,
        startTime + (distance * msPerCell_),
        startTime + msPerCell_,
        rStep,
        cStep,
        true
    };
    pm.pieceId = pieceId;
    pendingMoves_.push_back(pm);
}

void RealTimeArbiter::setPremove(uintptr_t pieceId, const Position& to) {
    premoves_[pieceId] = to;
}

int RealTimeArbiter::cooldownRemainingMs(uintptr_t pieceId) const {
    for (const auto& entry : cooldowns_) {
        if (entry.pieceId == pieceId) {
            return std::max(0, entry.endTimeMs - currentTimeMs_);
        }
    }
    return 0;
}

void RealTimeArbiter::beginAirborne(uintptr_t pieceId) {
    airborneEntries_.push_back({pieceId, currentTimeMs_ + airborneMs_});
}

void RealTimeArbiter::resolveAirborneCounterKill(PendingMove& pm, Piece* attacker, Piece* airbornePiece) {
    const bool attackerWasKing = (attacker->type() == PieceType::King);
    board_->removePiece(pm.currentPos);
    pm.active = false;
    airbornePiece->setState(PieceState::Idle);
    if (attackerWasKing) {
        kingCaptured_ = true;
    }
}

void RealTimeArbiter::resolveAirborneExpirations() {
    for (auto it = airborneEntries_.begin(); it != airborneEntries_.end();) {
        if (it->endTimeMs != currentTimeMs_) {
            ++it;
            continue;
        }

        const uintptr_t pieceId = it->pieceId;
        it = airborneEntries_.erase(it);

        Piece* piece = nullptr;
        for (Piece* candidate : board_->pieces()) {
            if (candidate && static_cast<uintptr_t>(candidate->id()) == pieceId) {
                piece = candidate;
                break;
            }
        }
        if (!piece || piece->state() != PieceState::Airborne) {
            continue;  // already landed some other way (e.g. counter-kill)
        }
        piece->setState(PieceState::Idle);
    }
}

void RealTimeArbiter::resolveCooldownExpirations() {
    for (auto it = cooldowns_.begin(); it != cooldowns_.end();) {
        if (it->endTimeMs != currentTimeMs_) {
            ++it;
            continue;
        }

        const uintptr_t pieceId = it->pieceId;
        it = cooldowns_.erase(it);

        Piece* piece = nullptr;
        for (Piece* candidate : board_->pieces()) {
            if (candidate && static_cast<uintptr_t>(candidate->id()) == pieceId) {
                piece = candidate;
                break;
            }
        }
        if (!piece || piece->state() != PieceState::Cooldown) {
            continue;
        }
        piece->setState(PieceState::Idle);

        auto premoveIt = premoves_.find(pieceId);
        if (premoveIt == premoves_.end()) {
            continue;
        }
        const Position to = premoveIt->second;
        premoves_.erase(premoveIt);

        if (!ruleEngine_) {
            continue;
        }
        const auto validation = ruleEngine_->validateMove(piece->position(), to);
        if (!validation.is_valid) {
            continue;  // invalid premove just fizzles - this is also how a
                       // deliberately-illegal request cancels a real one.
        }

        piece->setState(PieceState::Moving);
        startMove(piece->position(), to, pieceId);
    }
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

                    if (target->state() == PieceState::Airborne) {
                        resolveAirborneCounterKill(pm, currentPiece, target);
                        if (kingCaptured_) {
                            return;
                        }
                        continue;
                    }

                    const bool capturedKing = (target->type() == PieceType::King);
                    board_->removePiece(pm.to);
                    board_->movePiece(pm.currentPos, pm.to);
                    pm.currentPos = pm.to;
                    if (capturedKing) {
                        kingCaptured_ = true;
                        return;
                    }
                    beginCooldown(currentPiece);
                    continue;
                }

                board_->movePiece(pm.currentPos, pm.to);
                pm.currentPos = pm.to;
                beginCooldown(currentPiece);
                continue;
            }

            if (currentTimeMs_ == pm.nextStepTimeMs || currentTimeMs_ == pm.arrivalTimeMs) {
                Position nextPos(pm.currentPos.row() + pm.rowStep, pm.currentPos.col() + pm.colStep);
                bool capturedThisStep = false;

                auto targetPieceOpt = board_->pieceAt(nextPos);
                if (targetPieceOpt.has_value() && targetPieceOpt.value() != nullptr) {
                    Piece* target = targetPieceOpt.value();

                    // Is the piece already sitting on the next square itself
                    // mid-move (does it have its own active PendingMove)?
                    PendingMove* otherPm = nullptr;
                    for (auto& opm : pendingMoves_) {
                        if (opm.active && opm.currentPos == nextPos) {
                            otherPm = &opm;
                            break;
                        }
                    }

                    if (otherPm != nullptr) {
                        // Both pieces are actively moving and their paths just crossed.
                        if (target->color() == currentPiece->color()) {
                            // Same color: the piece trying to enter stops in
                            // place; whichever piece was already there keeps
                            // going, undisturbed.
                            currentPiece->setState(PieceState::Idle);
                            pm.active = false;
                            continue;
                        } else {
                            // Enemy: whichever piece's step *enters* the
                            // square and finds it already occupied wins,
                            // regardless of which of the two started moving
                            // first. The winner stops right here rather than
                            // continuing toward its own original target.
                            target->setState(PieceState::Captured);
                            board_->removePiece(nextPos);
                            otherPm->active = false;

                            board_->movePiece(pm.currentPos, nextPos);
                            pm.currentPos = nextPos;
                            capturedThisStep = true;
                        }
                    } else {
                        // The piece at the target square isn't itself moving.
                        if (target->color() == currentPiece->color()) {
                            currentPiece->setState(PieceState::Idle);
                            pm.active = false;
                            continue;
                        } else {
                            if (target->state() == PieceState::Airborne) {
                                // An airborne piece is immune: it lands and
                                // eliminates the attacker instead of being
                                // captured like an ordinary stationary piece.
                                resolveAirborneCounterKill(pm, currentPiece, target);
                                if (kingCaptured_) {
                                    return;
                                }
                                continue;
                            }

                            // Capturing a stationary enemy ends the slide
                            // here, exactly like standard chess.
                            bool capturedKing = (target->type() == PieceType::King);

                            // Any other pending move that was also racing
                            // toward this now-captured square stops too.
                            for (auto& otherPm2 : pendingMoves_) {
                                if (otherPm2.active && otherPm2.currentPos == nextPos) {
                                    otherPm2.active = false;
                                }
                            }

                            board_->removePiece(nextPos);
                            board_->movePiece(pm.currentPos, nextPos);
                            pm.currentPos = nextPos;
                            capturedThisStep = true;

                            if (capturedKing) {
                                kingCaptured_ = true;
                                pm.active = false;
                                return;
                            }
                        }
                    }
                } else {
                    // Nothing occupies the next square - just advance.
                    board_->movePiece(pm.currentPos, nextPos);
                    pm.currentPos = nextPos;
                }

                // A capture ends the move right here, even short of the
                // originally requested target. Otherwise, check whether this
                // step reached the final destination.
                if (capturedThisStep || pm.currentPos == pm.to || currentTimeMs_ >= pm.arrivalTimeMs) {
                    pm.active = false;
                    if (auto arrivedPiece = board_->pieceAt(pm.currentPos); arrivedPiece.has_value()) {
                        beginCooldown(arrivedPiece.value());
                    }
                    promoteIfNeeded(pm.currentPos);
                } else {
                    pm.nextStepTimeMs += msPerCell_;
                }
            }
        }

        pendingMoves_.erase(
            std::remove_if(pendingMoves_.begin(), pendingMoves_.end(),
                           [](const PendingMove& pm) { return !pm.active; }),
            pendingMoves_.end());

        resolveCooldownExpirations();
        resolveAirborneExpirations();

        if (kingCaptured_) return;
    }
}

std::vector<PendingMove> RealTimeArbiter::snapshotPendingMoves() const {
    return pendingMoves_;
}

}  // namespace kungfu
