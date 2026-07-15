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
#include "game/RealTimeArbiter.hpp"
#include <unordered_map>
#include <cstdint>

namespace kungfu {

GameEngine::GameEngine() : GameEngine(std::make_shared<Board>(), nullptr) {}

GameEngine::GameEngine(std::shared_ptr<IBoard> board) : GameEngine(std::move(board), nullptr) {}

GameEngine::GameEngine(std::shared_ptr<IBoard> board, std::shared_ptr<IRuleEngine> ruleEngine)
    : state_(GameState::NotStarted),
      board_(std::move(board)),
      ruleEngine_(std::move(ruleEngine)),
      arbiter_(std::make_unique<RealTimeArbiter>(board_)) {
    if (!ruleEngine_) {
        ruleEngine_ = std::make_shared<RuleEngine>(board_);
    }
}

MoveResult GameEngine::requestMove(const Position& from, const Position& to) {
    if (state_ != GameState::Running) {
        return {false, "game_over"};
    }

    const auto validation = ruleEngine_->validateMove(from, to);
    if (!validation.is_valid) {
        return {false, validation.reason};
    }

    const auto movingPieceOpt = board_->pieceAt(from);
    if (!movingPieceOpt.has_value() || movingPieceOpt.value() == nullptr) {
        return {false, "empty_source"};
    }
    Piece* movingPiece = movingPieceOpt.value();

    if (movingPiece->state() == PieceState::Moving) {
        return {false, "piece_already_moving"};
    }

    int rowDelta = to.row() - from.row();
    int colDelta = to.col() - from.col();
    int distance = std::max(std::abs(rowDelta), std::abs(colDelta));
    int rStep = (rowDelta == 0) ? 0 : (rowDelta > 0 ? 1 : -1);
    int cStep = (colDelta == 0) ? 0 : (colDelta > 0 ? 1 : -1);

    int startTime = arbiter_->currentTimeMs();
    PendingMove pm{
        from,
        to,
        from,
        startTime,
        startTime + (distance * GameConfig::kMsPerCell),
        startTime + GameConfig::kMsPerCell,
        rStep,
        cStep,
        true
    };
    // attach piece id so Arbiter and UI can correlate pending moves to pieces
    pm.pieceId = static_cast<uintptr_t>(movingPiece->id());

    arbiter_->addMove(pm);
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

    for (auto& rp : render) {
        // default cooldown
        rp.cooldownMs = 0.0;

        auto it = map.find(rp.id);
        if (it == map.end()) continue;
        const PendingMove& pm = it->second;

        // Compute interpolation between pm.currentPos and next step/target
        double totalDuration = std::max(1, pm.arrivalTimeMs - pm.startTimeMs);
        double elapsed = std::clamp(now - pm.startTimeMs, 0, pm.arrivalTimeMs - pm.startTimeMs);

        // linear interpolation from start->to over totalDuration
        double t = totalDuration > 0 ? (elapsed / totalDuration) : 1.0;

        double sx = pm.from.row();
        double sy = pm.from.col();
        double ex = pm.to.row();
        double ey = pm.to.col();

        rp.x = sx + (ex - sx) * t;
        rp.y = sy + (ey - sy) * t;

        // cooldown: time until arrival
        int remaining = pm.arrivalTimeMs - now;
        rp.cooldownMs = remaining > 0 ? static_cast<double>(remaining) : 0.0;
    }

    return render;
}

std::string GameEngine::getPieceToken(const Piece* piece) const {
    if (!piece) return ".";
    std::string token = (piece->color() == PlayerColor::White) ? "w" : "b";
    switch (piece->type()) {
        case PieceType::King:   token += "K"; break;
        case PieceType::Queen:  token += "Q"; break;
        case PieceType::Rook:   token += "R"; break;
        case PieceType::Bishop: token += "B"; break;
        case PieceType::Knight: token += "N"; break;
        case PieceType::Pawn:   token += "P"; break;
    }
    return token;
}

void GameEngine::wait(int ms) {
    if (state_ != GameState::Running) {
        return;
    }

    arbiter_->advanceTime(ms);

    if (arbiter_->isKingCaptured()) {
        state_ = GameState::Finished;
    }
}

void GameEngine::printBoard(std::ostream& out) const {
    int maxRows = boardRows();
    int maxCols = boardCols();

    for (int r = 0; r < maxRows; ++r) {
        for (int c = 0; c < maxCols; ++c) {
            auto piece = board_->pieceAt(Position(r, c));
            if (piece.has_value()) {
                out << getPieceToken(piece.value());
            } else {
                out << ".";
            }
            if (c + 1 < maxCols) {
                out << " ";
            }
        }
        out << "\n";
    }
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
        pieceState == PieceState::Captured) {
        return false;
    }

    piece.value()->setState(PieceState::Airborne);
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
