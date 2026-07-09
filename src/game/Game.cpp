#include "game/Game.hpp"
#include "board/Board.hpp"
#include "common/GameConfig.hpp"
#include "pieces/Queen.hpp"
#include "rules/RuleEngine.hpp"

namespace kungfu {

Game::Game() : Game(std::make_shared<Board>(), nullptr) {}

Game::Game(std::shared_ptr<IBoard> board) : Game(std::move(board), nullptr) {}

Game::Game(std::shared_ptr<IBoard> board, std::shared_ptr<IRuleEngine> ruleEngine)
    : state_(GameState::NotStarted),
      board_(std::move(board)),
      ruleEngine_(std::move(ruleEngine)),
      collisionSystem_(std::make_shared<CollisionSystem>(board_)) {
    if (!ruleEngine_) {
        ruleEngine_ = std::make_shared<RuleEngine>(board_);
    }
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
    if (state_ != GameState::Running || pendingMove_.has_value()) {
        return;
    }

    // המרת פיקסלים לתאי לוח (כל תא 100x100)
    Position pos{y / 100, x / 100};

    // התעלמות מלחיצה מחוץ לגבולות הלוח
    auto concreteBoard = std::dynamic_pointer_cast<Board>(board_);
    int maxRows = concreteBoard ? concreteBoard->rows() : GameConfig::kBoardSize;
    int maxCols = concreteBoard ? concreteBoard->cols() : GameConfig::kBoardSize;
    if (!movementSystem_.isInBounds(pos, maxRows, maxCols)) {
        return;
    }

    auto piece = board_->pieceAt(pos);

    // אם אין כלי מסומן כרגע
    if (!selectedPosition_.has_value()) {
        if (piece.has_value()) {
            selectedPosition_ = pos; // בחירת הכלי
        }
        return;
    }

    // לחיצה על אותו תא מסומן (תומך בקפיצות לשלבים הבאים)
    if (pos == *selectedPosition_) {
        tryJump(pos);
        selectedPosition_.reset();
        return;
    }

    // אם כבר יש כלי מסומן, ולחצנו על תא עם כלי אחר
    if (piece.has_value()) {
        auto selectedPiece = board_->pieceAt(*selectedPosition_);
        if (selectedPiece.has_value() && selectedPiece.value()->color() == piece.value()->color()) {
            // לחיצה על כלי ידידותי אחר -> מחליפה את הסימון
            selectedPosition_ = pos;
            return;
        }
    }

    // ניסיון לשלוח בקשת תנועה (זמן ההגעה מחושב לפי מרחק הצעדים - 1000 מילישניות לכל תא)
    if (ruleEngine_->isValidMove(*selectedPosition_, pos)) {
        int rowDelta = std::abs(pos.row() - selectedPosition_->row());
        int colDelta = std::abs(pos.col() - selectedPosition_->col());
        int distance = std::max(rowDelta, colDelta); // מרחק צעדים

        pendingMove_ = PendingMove{*selectedPosition_, pos, currentTimeMs_ + (distance * 1000)};

        // סימון הכלי כנמצא בתנועה (נועל אותו מפקודות נוספות)
        auto movingPiece = board_->pieceAt(*selectedPosition_);
        if (movingPiece.has_value()) {
            movingPiece.value()->setState(PieceState::Moving);
        }

        selectedPosition_.reset();
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

        // החזרת מצב הכלי ל-Idle כדי שיוכל לזוז שוב
        auto movingPiece = board_->pieceAt(from);
        if (movingPiece.has_value()) {
            movingPiece.value()->setState(PieceState::Idle);
        }

        tryMove(from, to); // ביצוע התנועה בפועל באופן מיידי
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
