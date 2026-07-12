#include "game/Game.hpp"
#include <algorithm>
#include <stdexcept>

#include "board/Board.hpp"
#include "common/GameConfig.hpp"
#include "game/UIInputAdapter.hpp"
#include "pieces/Queen.hpp"
#include "rules/RuleEngine.hpp"

namespace kungfu {

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
      collisionSystem_(std::make_shared<CollisionSystem>(board_)),
      inputAdapter_(std::move(inputAdapter)) {
    if (!ruleEngine_) {
        ruleEngine_ = std::make_shared<RuleEngine>(board_);
    }
    if (!inputAdapter_) {
        throw std::invalid_argument("Game requires an IGameInputAdapter");
    }
}

bool Game::selectPiece(const Position& pos) {
    if (state_ != GameState::Running || pendingMove_.has_value()) {
        return false;
    }

    const auto piece = board_->pieceAt(pos);
    if (!piece.has_value()) {
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
    if (state_ != GameState::Running || pendingMove_.has_value()) {
        return false;
    }

    if (!ruleEngine_->isValidMove(from, to)) {
        return false;
    }

    const auto movingPiece = board_->pieceAt(from);
    if (!movingPiece.has_value()) {
        return false;
    }

    if (movingPiece.value()->state() == PieceState::Moving) {
        return false;
    }

    if (collisionSystem_->isFriendlyBlock(from, to)) {
        return false;
    }

    PieceType type = movingPiece.value()->type();
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
        targetPiece.value()->color() == movingPiece.value()->color()) {
        return false;
    }

    if (targetPiece.has_value() && targetPiece.value()->isAirborne() &&
        targetPiece.value()->color() != movingPiece.value()->color()) {
        movingPiece.value()->setState(PieceState::Captured);
        board_->removePiece(from);
        selectedPosition_.reset();
        return true;
    }

    const bool capturedKing = collisionSystem_->isCapture(from, to) &&
                              targetPiece.has_value() &&
                              targetPiece.value()->type() == PieceType::King;

    int rowDelta = std::abs(to.row() - from.row());
    int colDelta = std::abs(to.col() - from.col());
    int distance = std::max(rowDelta, colDelta);

    pendingMove_ = PendingMove{from, to, currentTimeMs_ + (distance * 1000)};
    movingPiece.value()->setState(PieceState::Moving);
    selectedPosition_.reset();

    if (capturedKing) {
        // The king capture will be resolved when the move completes.
        // Keep the pending move active until arrival so the piece remains visible while moving.
        return true;
    }

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

std::string Game::getPieceToken(const PiecePtr& piece) const {
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

// מימוש פקודת click x y
void Game::click(int x, int y) {
    if (inputAdapter_) {
        inputAdapter_->handleClick(x, y);
    }
}

// מימוש פקודת wait ms
void Game::wait(int ms) {
    if (state_ != GameState::Running) {
        return;
    }
    currentTimeMs_ += ms;

    // בדיקה האם הגיע זמן פתרון התנועה המושהית
    if (pendingMove_.has_value() && currentTimeMs_ >= pendingMove_->arrivalTimeMs) {
        Position from = pendingMove_->from;
        Position to = pendingMove_->to;
        pendingMove_.reset();

        auto movingPiece = board_->pieceAt(from);
        if (!movingPiece.has_value()) {
            return;
        }

        const auto targetPiece = board_->pieceAt(to);
        const bool capturedKing = collisionSystem_->isCapture(from, to) &&
                                  targetPiece.has_value() &&
                                  targetPiece.value()->type() == PieceType::King;

        board_->movePiece(from, to);

        if (capturedKing) {
            state_ = GameState::Finished;
            return;
        }

        if (auto movedPiece = board_->pieceAt(to); movedPiece.has_value()) {
            movedPiece.value()->setState(PieceState::Idle);
            if (movedPiece.value()->type() == PieceType::Pawn &&
                ruleEngine_->isPawnPromotion(to, movedPiece.value()->color())) {
                board_->replacePiece(to, std::make_shared<Queen>(movedPiece.value()->color(), to));
            }
        }
    }
}

// מימוש פקודת print board
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

    // A moving piece cannot initiate a second move.
    const auto sourcePiece = board_->pieceAt(from);
    if (sourcePiece.has_value() && sourcePiece.value()->state() == PieceState::Moving) {
        return false;
    }

    if (collisionSystem_->isFriendlyBlock(from, to)) {
        return false;
    }

    // --- עדכון עבור כלים מחליקים (מלכה, צריח, רץ) ---
    // ודואים שהנתיב שלהם פנוי מכלים אחרים (פרש ומלך פטורים מבדיקה זו)
    if (sourcePiece.has_value()) {
        PieceType type = sourcePiece.value()->type();
        if (type == PieceType::Queen || type == PieceType::Rook || type == PieceType::Bishop) {
            if (!collisionSystem_->isPathClear(from, to)) {
                return false; // הנתיב חסום על ידי כלי אחר
            }
        }
    }

    // For a double-step pawn move, the intermediate square must be empty.
    const auto middle = movementSystem_.pawnDoubleStepMiddle(from, to);
    if (middle.has_value() && !collisionSystem_->isPathClear(from, to)) {
        return false;
    }

    // If the destination holds an airborne friendly piece, it blocks the move.
    const auto targetPiece = board_->pieceAt(to);
    if (targetPiece.has_value() && targetPiece.value()->isAirborne() &&
        targetPiece.value()->color() == sourcePiece.value()->color()) {
        return false;
    }

    // If an enemy piece is airborne at the destination, it captures the arriving piece.
    if (targetPiece.has_value() && targetPiece.value()->isAirborne() &&
        targetPiece.value()->color() != sourcePiece.value()->color()) {
        // Airborne captures: remove the arriving piece, airborne stays.
        sourcePiece.value()->setState(PieceState::Captured);
        board_->removePiece(from);
        return true;
    }

    // Record whether we are capturing a king before movePiece removes it.
    const bool capturedKing = collisionSystem_->isCapture(from, to) &&
                              targetPiece.has_value() &&
                              targetPiece.value()->type() == PieceType::King;

    if (!board_->movePiece(from, to)) {
        return false;
    }

    if (capturedKing) {
        state_ = GameState::Finished;
        return true;
    }

    // Delegate promotion decision and piece replacement to the rule engine.
    const auto movedPiece = board_->pieceAt(to);
    if (movedPiece.has_value() &&
        movedPiece.value()->type() == PieceType::Pawn &&
        ruleEngine_->isPawnPromotion(to, movedPiece.value()->color())) {
        board_->replacePiece(to, std::make_shared<Queen>(movedPiece.value()->color(), to));
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
    // A moving or already-airborne or captured piece cannot jump.
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
    // No enemy arrived — piece lands normally (state back to Idle).
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

    // Same color — no capture.
    if (airbornePiece.value()->color() == arrivingPiece.value()->color()) {
        return false;
    }

    // Airborne captures the arriving enemy: remove it, airborne piece stays.
    board_->removePiece(arrivingFrom);
    airbornePiece.value()->setState(PieceState::Idle);

    if (arrivingPiece.value()->type() == PieceType::King) {
        state_ = GameState::Finished;
    }

    return true;
}

}  // namespace kungfu
