#include "game/Game.hpp"
#include <algorithm>
#include <stdexcept>
#include <cmath>

#include "board/Board.hpp"
#include "common/GameConfig.hpp"
#include "game/UIInputAdapter.hpp"
#include "pieces/Queen.hpp"
#include "rules/RuleEngine.hpp"

namespace kungfu {
namespace {

bool captureEnemyAtDestination(IBoard& board, const Position& to, const Piece* movingPiece) {
    const auto targetPiece = board.pieceAt(to);
    if (!targetPiece.has_value() || !targetPiece.value()) {
        return false;
    }

    if (targetPiece.value()->color() == movingPiece->color()) {
        return false;
    }

    return board.removePiece(to);
}

}  // namespace

Game::Game() : Game(std::make_shared<Board>(), nullptr, nullptr) {}

Game::Game(std::shared_ptr<IBoard> board) : Game(std::move(board), nullptr, nullptr) {}

Game::Game(std::shared_ptr<IBoard> board, std::shared_ptr<IRuleEngine> ruleEngine)
    : Game(std::move(board), std::move(ruleEngine), nullptr) {}

Game::Game(std::shared_ptr<IBoard> board,
           std::shared_ptr<IRuleEngine> ruleEngine,
           IGameInputAdapterPtr inputAdapter)
    : state_(GameState::NotStarted),
      board_(std::move(board)),
      ruleEngine_(std::move(ruleEngine)),
      collisionSystem_(std::make_unique<CollisionSystem>(board_)),
      inputAdapter_(std::move(inputAdapter)) {
    if (!ruleEngine_) {
        ruleEngine_ = std::make_shared<RuleEngine>(board_);
    }
    if (!inputAdapter_) {
        throw std::invalid_argument("Game requires an IGameInputAdapter");
    }
}

bool Game::selectPiece(const Position& pos) {
    if (state_ != GameState::Running) {
        return false;
    }

    const auto piece = board_->pieceAt(pos);
    if (!piece.has_value()) {
        return false;
    }

    // מניעת בחירה של כלי שנמצא כרגע בתנועה
    if (piece.value()->state() == PieceState::Moving) {
        return false;
    }

    if (!selectedPosition_.has_value()) {
        selectedPosition_ = pos;
        return true;
    }

    if (pos == *selectedPosition_) {
        requestJump(pos);
        selectedPosition_.reset();
        return true;
    }

    const auto selectedPiece = board_->pieceAt(*selectedPosition_);
    if (selectedPiece.has_value() && selectedPiece.value()->color() == piece.value()->color()) {
        selectedPosition_ = pos;
        return true;
    }

    return false;
}

bool Game::requestMove(const Position& from, const Position& to) {
    if (state_ != GameState::Running) {
        return false;
    }

    if (!ruleEngine_->isValidMove(from, to)) {
        return false;
    }

    const auto movingPieceOpt = board_->pieceAt(from);
    if (!movingPieceOpt.has_value() || movingPieceOpt.value() == nullptr) {
        return false;
    }
    Piece* movingPiece = movingPieceOpt.value();

    if (movingPiece->state() == PieceState::Moving) {
        return false;
    }

    if (collisionSystem_->isFriendlyBlock(from, to)) {
        return false;
    }

    PieceType type = movingPiece->type();
    if (type == PieceType::Queen || type == PieceType::Rook || type == PieceType::Bishop) {
        if (!collisionSystem_->isPathClear(from, to)) {
            return false;
        }
    }

    const auto middle = movementSystem_.pawnDoubleStepMiddle(from, to);
    if (middle.has_value() && !collisionSystem_->isPathClear(from, to)) {
        return false;
    }

    int rowDelta = to.row() - from.row();
    int colDelta = to.col() - from.col();
    int distance = std::max(std::abs(rowDelta), std::abs(colDelta));
    int rStep = (rowDelta == 0) ? 0 : (rowDelta > 0 ? 1 : -1);
    int cStep = (colDelta == 0) ? 0 : (colDelta > 0 ? 1 : -1);

    // יצירת תנועה מושהית חדשה והכנסתה למאגר התנועות הפעילות במקביל
    PendingMove pm{
        from,
        to,
        from,
        currentTimeMs_,
        currentTimeMs_ + (distance * 1000),
        currentTimeMs_ + 1000,
        rStep,
        cStep,
        true
    };

    pendingMoves_.push_back(pm);
    movingPiece->setState(PieceState::Moving);
    selectedPosition_.reset();

    return true;
}

bool Game::requestJump(const Position& pos) {
    const bool jumped = tryJump(pos);
    if (jumped) {
        selectedPosition_.reset();
    }
    return jumped;
}

bool Game::hasSelection() const {
    return selectedPosition_.has_value();
}

std::optional<Position> Game::selectedPosition() const {
    return selectedPosition_;
}

bool Game::isFriendlyPieceAt(const Position& pos) const {
    if (!selectedPosition_.has_value()) {
        return false;
    }

    const auto selectedPiece = board_->pieceAt(*selectedPosition_);
    const auto targetPiece = board_->pieceAt(pos);
    return selectedPiece.has_value() && targetPiece.has_value() &&
           selectedPiece.value()->color() == targetPiece.value()->color();
}

bool Game::isPositionInBounds(const Position& pos) const {
    auto concreteBoard = std::dynamic_pointer_cast<Board>(board_);
    int maxRows = concreteBoard ? concreteBoard->rows() : GameConfig::kBoardSize;
    int maxCols = concreteBoard ? concreteBoard->cols() : GameConfig::kBoardSize;
    return movementSystem_.isInBounds(pos, maxRows, maxCols);
}

void Game::start() {
    state_ = GameState::Running;
}

void Game::stop() {
    state_ = GameState::Paused;
}

bool Game::isRunning() const {
    return state_ == GameState::Running;
}

bool Game::isFinished() const {
    return state_ == GameState::Finished;
}

std::shared_ptr<IBoard> Game::getBoard() const {
    return board_;
}

std::string Game::getPieceToken(const Piece* piece) const {
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

void Game::click(int x, int y) {
    if (inputAdapter_) {
        inputAdapter_->handleClick(x, y);
    }
}

// לולאת ניהול הזמן וההתנגשויות המשוכללת בזמן אמת (Tick-by-Tick)
void Game::wait(int ms) {
    if (state_ != GameState::Running) {
        return;
    }

    for (int t = 0; t < ms; ++t) {
        currentTimeMs_++;

        for (size_t i = 0; i < pendingMoves_.size(); ++i) {
            auto& pm = pendingMoves_[i];
            if (!pm.active) continue;

            // בדיקה האם הגיע הזמן לבצע את הצעד הבא או להגיע ליעד
            if (currentTimeMs_ == pm.nextStepTimeMs || currentTimeMs_ == pm.arrivalTimeMs) {
                Position nextPos(pm.currentPos.row() + pm.rowStep, pm.currentPos.col() + pm.colStep);

                auto targetPieceOpt = board_->pieceAt(nextPos);
                if (targetPieceOpt.has_value()) {
                    Piece* target = targetPieceOpt.value();
                    auto currentPieceOpt = board_->pieceAt(pm.currentPos);
                    
                    if (currentPieceOpt.has_value() && target->color() == currentPieceOpt.value()->color()) {
                        // --- חוק 1: כלים מאותו צבע נפגשים ---
                        // הכלי שהגיע מאוחר יותר נתקע במשבצת הקודמת (currentPos) וחוזר למצב Idle
                        currentPieceOpt.value()->setState(PieceState::Idle);
                        pm.active = false;
                        continue;
                    } else {
                        // --- חוק 2: כלים מצבעים שונים נפגשים ---
                        // הכלי שהגיע מאוחר יותר אוכל את הכלי שהגיע קודם
                        bool capturedKing = (target->type() == PieceType::King);

                        // ביטול התנועה של הכלי שנאכל אם הוא היה באמצע תנועה
                        for (auto& otherPm : pendingMoves_) {
                            if (otherPm.active && otherPm.currentPos == nextPos) {
                                otherPm.active = false;
                            }
                        }

                        board_->removePiece(nextPos);
                        board_->movePiece(pm.currentPos, nextPos);
                        pm.currentPos = nextPos;

                        if (capturedKing) {
                            state_ = GameState::Finished;
                            pm.active = false;
                            return;
                        }
                    }
                } else {
                    // המשבצת הבאה פנויה - הכלי מתקדם כרגיל על הלוח
                    board_->movePiece(pm.currentPos, nextPos);
                    pm.currentPos = nextPos;
                }

                // בדיקת סיום מסלול והגעה ליעד הסופי
                if (pm.currentPos == pm.to || currentTimeMs_ >= pm.arrivalTimeMs) {
                    pm.active = false;
                    if (auto movedPiece = board_->pieceAt(pm.to); movedPiece.has_value()) {
                        movedPiece.value()->setState(PieceState::Idle);
                        if (movedPiece.value()->type() == PieceType::Pawn &&
                            ruleEngine_->isPawnPromotion(pm.to, movedPiece.value()->color())) {
                            board_->replacePiece(pm.to, std::make_unique<Queen>(movedPiece.value()->color(), pm.to));
                        }
                    }
                } else {
                    pm.nextStepTimeMs += 1000;
                }
            }
        }

        // ניקוי תנועות שהסתיימו או בוטלו מהווקטור
        pendingMoves_.erase(
            std::remove_if(pendingMoves_.begin(), pendingMoves_.end(),
                           [](const PendingMove& pm) { return !pm.active; }),
            pendingMoves_.end());

        if (state_ == GameState::Finished) return;
    }
}

void Game::printBoard(std::ostream& out) const {
    auto concreteBoard = std::dynamic_pointer_cast<Board>(board_);
    int maxRows = concreteBoard ? concreteBoard->rows() : GameConfig::kBoardSize;
    int maxCols = concreteBoard ? concreteBoard->cols() : GameConfig::kBoardSize;

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

bool Game::tryMove(const Position& from, const Position& to) {
    if (state_ != GameState::Running) {
        return false;
    }

    if (!ruleEngine_->isValidMove(from, to)) {
        return false;
    }

    const auto sourcePieceOpt = board_->pieceAt(from);
    if (sourcePieceOpt.has_value() && sourcePieceOpt.value()->state() == PieceState::Moving) {
        return false;
    }

    Piece* sourcePiece = sourcePieceOpt.has_value() ? sourcePieceOpt.value() : nullptr;
    if (sourcePiece == nullptr) {
        return false;
    }

    if (collisionSystem_->isFriendlyBlock(from, to)) {
        return false;
    }

    PieceType type = sourcePiece->type();
    if (type == PieceType::Queen || type == PieceType::Rook || type == PieceType::Bishop) {
        if (!collisionSystem_->isPathClear(from, to)) {
            return false;
        }
    }

    const auto middle = movementSystem_.pawnDoubleStepMiddle(from, to);
    if (middle.has_value() && !collisionSystem_->isPathClear(from, to)) {
        return false;
    }

    const auto targetPiece = board_->pieceAt(to);
    if (targetPiece.has_value() && targetPiece.value()->isAirborne() &&
        targetPiece.value()->color() == sourcePiece->color()) {
        return false;
    }

    if (targetPiece.has_value() && targetPiece.value()->isAirborne() &&
        targetPiece.value()->color() != sourcePiece->color()) {
        sourcePiece->setState(PieceState::Captured);
        board_->removePiece(from);
        return true;
    }

    bool capturedKing = false;
    if (targetPiece.has_value() && targetPiece.value() &&
        targetPiece.value()->color() != sourcePiece->color()) {
        capturedKing = targetPiece.value()->type() == PieceType::King;
        captureEnemyAtDestination(*board_, to, sourcePiece);
    }

    if (!board_->movePiece(from, to)) {
        return false;
    }

    if (capturedKing) {
        state_ = GameState::Finished;
        return true;
    }

    const auto movedPiece = board_->pieceAt(to);
    if (movedPiece.has_value() &&
        movedPiece.value()->type() == PieceType::Pawn &&
        ruleEngine_->isPawnPromotion(to, movedPiece.value()->color())) {
        board_->replacePiece(to, std::make_unique<Queen>(movedPiece.value()->color(), to));
    }

    return true;
}

bool Game::tryJump(const Position& cell) {
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

void Game::resolveJump(const Position& cell) {
    const auto piece = board_->pieceAt(cell);
    if (!piece.has_value() || !piece.value()->isAirborne()) {
        return;
    }
    piece.value()->setState(PieceState::Idle);
}

bool Game::handleArrivalAtAirbornCell(const Position& cell, const Position& arrivingFrom) {
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

}  // namespace kungfu