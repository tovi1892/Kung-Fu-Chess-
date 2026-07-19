#include "engine/GameEngine.hpp"
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <utility>
#include <memory>

#include "model/Board.hpp"
#include "IGameView.hpp"
#include "model/GameConfig.hpp"
#include "rules/RuleEngine.hpp"
#include "realtime/RealTimeArbiter.hpp"
#include <unordered_map>
#include <cstdint>

namespace kungfu {

GameEngine::GameEngine() : GameEngine(std::make_shared<Board>(), nullptr) {}

GameEngine::GameEngine(std::shared_ptr<IBoard> board) : GameEngine(std::move(board), nullptr) {}

GameEngine::GameEngine(std::shared_ptr<IBoard> board, std::shared_ptr<IRuleEngine> ruleEngine,
                        double speedMultiplier)
    : state_(GameState::NotStarted),
      board_(std::move(board)),
      ruleEngine_(std::move(ruleEngine)) {
    if (!ruleEngine_) {
        ruleEngine_ = std::make_shared<RuleEngine>(board_);
    }
    arbiter_ = std::make_unique<RealTimeArbiter>(board_, ruleEngine_, captureBus_, speedMultiplier);

    // The only subscriber GameEngine wires up on itself: every other consumer (UI score
    // panel, move log, sound, banners) subscribes from the outside via onPieceCaptured()
    // etc. This one updates GameRecord (Logic's own ledger) and detects game-over -
    // both are GameEngine's own bookkeeping, not something an external subscriber should
    // have to reproduce.
    captureBus_.subscribe([this](const PieceCaptured& event) {
        record_.recordCapture(event.capturingColor, event.capturedType);
        if (event.capturedType == PieceType::King) {
            state_ = GameState::Finished;
            endBus_.publish({event.capturingColor});
        }
    });
}

MoveResult GameEngine::requestMove(const Position& from, const Position& to) {
    if (state_ != GameState::Running) {
        return {false, "game_over"};
    }

    const auto movingPieceOpt = board_->pieceAt(from);
    if (!movingPieceOpt.has_value() || movingPieceOpt.value() == nullptr) {
        return {false, "empty_source"};
    }
    Piece* movingPiece = movingPieceOpt.value();

    if (movingPiece->state() == PieceState::Moving) {
        return {false, "piece_already_moving"};
    }

    if (movingPiece->state() == PieceState::Airborne) {
        return {false, "piece_airborne"};
    }

    if (movingPiece->state() == PieceState::Cooldown || movingPiece->state() == PieceState::ShortRest) {
        // Queued unvalidated: this is also how submitting a deliberately
        // illegal request cancels a previously queued premove (it overwrites
        // the same slot, and will itself fail validation when the rest ends).
        arbiter_->setPremove(static_cast<uintptr_t>(movingPiece->id()), to);
        return {true, "premove_queued"};
    }

    const auto validation = ruleEngine_->validateMove(from, to);
    if (!validation.is_valid) {
        return {false, validation.reason};
    }

    // Recorded from the request itself (from/to/whether 'to' looked like a
    // capture right now), not wherever real-time resolution actually ends
    // the move - a simplification, not a full PGN engine.
    const auto targetPieceOpt = board_->pieceAt(to);
    const bool isCapture = targetPieceOpt.has_value() && targetPieceOpt.value() != nullptr &&
                            targetPieceOpt.value()->color() != movingPiece->color();
    const std::string notation = moveNotation(movingPiece->type(), from, to, isCapture);
    const int elapsedMs = arbiter_->currentTimeMs();
    record_.recordMove(movingPiece->color(), elapsedMs, notation);
    moveBus_.publish({movingPiece->color(), movingPiece->type(), from, to, isCapture, notation, elapsedMs});

    arbiter_->startMove(from, to, static_cast<uintptr_t>(movingPiece->id()));
    movingPiece->setState(PieceState::Moving);

    return {true, "ok"};
}

bool GameEngine::requestJump(const Position& pos) {
    return tryJump(pos);
}

bool GameEngine::isPositionInBounds(const Position& pos) const {
    return movementSystem_.isInBounds(pos, boardRows(), boardCols());
}

int GameEngine::boardRows() const {
    return board_ ? board_->rows() : GameConfig::kBoardSize;
}

int GameEngine::boardCols() const {
    return board_ ? board_->cols() : GameConfig::kBoardSize;
}

void GameEngine::start() {
    state_ = GameState::Running;
    startBus_.publish({});
}

void GameEngine::stop() {
    state_ = GameState::Paused;
}

bool GameEngine::isRunning() const {
    return state_ == GameState::Running;
}

bool GameEngine::isFinished() const {
    return state_ == GameState::Finished;
}

std::shared_ptr<IBoard> GameEngine::getBoard() const {
    return board_;
}

std::vector<RenderPiece> GameEngine::getRenderState() const {
    // Delegate to board and augment with arbiter-provided interpolations
    auto render = std::dynamic_pointer_cast<Board>(board_)->getRenderState();
    if (!arbiter_) return render;

    const int now = arbiter_->currentTimeMs();
    auto pending = arbiter_->snapshotPendingMoves();

    // Build a map from pieceId to PendingMove for quick lookup
    std::unordered_map<uintptr_t, PendingMove> map;
    for (const auto& pm : pending) {
        if (pm.active) map[pm.pieceId] = pm;
    }

    // A view only ever needs "how long until this piece is available again",
    // not why - both the post-move cooldown and the post-jump short rest
    // report through this same pair of RenderPiece fields.
    auto applyRestProgress = [](RenderPiece& rp, int remainingMs, int totalMs) {
        rp.cooldownMs = static_cast<double>(remainingMs);
        rp.cooldownTotalMs = static_cast<double>(totalMs);
    };

    for (auto& rp : render) {
        rp.cooldownMs = 0.0;
        rp.cooldownTotalMs = 0.0;

        if (auto it = map.find(rp.id); it != map.end()) {
            // Mid-move: interpolate position. No cooldown data while moving
            // - a view has nothing meaningful to show for "time to arrival".
            const PendingMove& pm = it->second;

            double totalDuration = std::max(1, pm.arrivalTimeMs - pm.startTimeMs);
            double elapsed = std::clamp(now - pm.startTimeMs, 0, pm.arrivalTimeMs - pm.startTimeMs);
            double t = totalDuration > 0 ? (elapsed / totalDuration) : 1.0;

            double sx = pm.from.row();
            double sy = pm.from.col();
            double ex = pm.to.row();
            double ey = pm.to.col();

            rp.x = sx + (ex - sx) * t;
            rp.y = sy + (ey - sy) * t;
        } else if (static_cast<PieceState>(rp.state) == PieceState::Cooldown) {
            applyRestProgress(rp, arbiter_->cooldownRemainingMs(rp.id), arbiter_->cooldownMs());
        } else if (static_cast<PieceState>(rp.state) == PieceState::ShortRest) {
            applyRestProgress(rp, arbiter_->shortRestRemainingMs(rp.id), arbiter_->shortRestMs());
        }
    }

    return render;
}

void GameEngine::wait(int ms) {
    if (state_ != GameState::Running) {
        return;
    }

    arbiter_->advanceTime(ms);
}

bool GameEngine::tryJump(const Position& cell) {
    if (state_ != GameState::Running) {
        return false;
    }

    const auto piece = board_->pieceAt(cell);
    if (!piece.has_value() || !piece.value()) {
        return false;
    }

    const PieceState pieceState = piece.value()->state();
    if (pieceState == PieceState::Moving ||
        pieceState == PieceState::Airborne ||
        pieceState == PieceState::Cooldown ||
        pieceState == PieceState::ShortRest ||
        pieceState == PieceState::Captured) {
        return false;
    }

    piece.value()->setState(PieceState::Airborne);
    arbiter_->beginAirborne(static_cast<uintptr_t>(piece.value()->id()));
    return true;
}

void GameEngine::resolveJump(const Position& cell) {
    const auto piece = board_->pieceAt(cell);
    if (!piece.has_value() || !piece.value()->isAirborne()) {
        return;
    }
    piece.value()->setState(PieceState::Idle);
}

bool GameEngine::handleArrivalAtAirbornCell(const Position& cell, const Position& arrivingFrom) {
    const auto airbornePiece = board_->pieceAt(cell);
    if (!airbornePiece.has_value() || !airbornePiece.value()->isAirborne()) {
        return false;
    }

    const auto arrivingPiece = board_->pieceAt(arrivingFrom);
    if (!arrivingPiece.has_value()) {
        return false;
    }

    if (airbornePiece.value()->color() == arrivingPiece.value()->color()) {
        return false;
    }

    board_->removePiece(arrivingFrom);
    airbornePiece.value()->setState(PieceState::Idle);

    if (arrivingPiece.value()->type() == PieceType::King) {
        state_ = GameState::Finished;
    }

    return true;
}

} // namespace kungfu
