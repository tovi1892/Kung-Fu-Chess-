// ============================================================================
// AUTOMATICALLY GENERATED SINGLE-FILE REPRESENTATION BY BUILD_SINGLE_CPP.PY
// ============================================================================

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>


// === Source: src/common/Position.cpp ===

namespace kungfu {

class Position {
public:
    Position();
    Position(int row, int col);

    int row() const;
    int col() const;

    bool operator==(const Position& other) const;
    bool operator!=(const Position& other) const;

private:
    int row_;
    int col_;
};

}  // namespace kungfu

namespace kungfu {

Position::Position() : row_(0), col_(0) {}

Position::Position(int row, int col) : row_(row), col_(col) {}

int Position::row() const {
    return row_;
}

int Position::col() const {
    return col_;
}

bool Position::operator==(const Position& other) const {
    return row_ == other.row_ && col_ == other.col_;
}

bool Position::operator!=(const Position& other) const {
    return !(*this == other);
}

}  // namespace kungfu


// === Source: src/pieces/Piece.cpp ===


namespace kungfu {

enum class PieceType {
    King,
    Queen,
    Rook,
    Bishop,
    Knight,
    Pawn
};

enum class PlayerColor {
    White,
    Black
};

enum class PieceState {
    Idle,       // On the board, not moving.
    Moving,     // En route to a destination.
    Airborne,   // Jumping: stays in place, captures any enemy that arrives.
    Captured    // Removed from the game.
};

}  // namespace kungfu

namespace kungfu {

class Piece {
public:
    Piece(PieceType type, PlayerColor color, Position position);
    virtual ~Piece() = default;

    PieceType type() const;
    PlayerColor color() const;
    Position position() const;
    PieceState state() const;

    void setPosition(const Position& position);
    void setState(PieceState state);

    bool isAirborne() const;

    virtual bool isMovable() const = 0;

protected:
    PieceType type_;
    PlayerColor color_;
    Position position_;
    PieceState state_ = PieceState::Idle;
};

using PiecePtr = std::shared_ptr<Piece>;

}  // namespace kungfu

namespace kungfu {

Piece::Piece(PieceType type, PlayerColor color, Position position)
    : type_(type), color_(color), position_(position), state_(PieceState::Idle) {}

PieceType Piece::type() const {
    return type_;
}

PlayerColor Piece::color() const {
    return color_;
}

Position Piece::position() const {
    return position_;
}

PieceState Piece::state() const {
    return state_;
}

void Piece::setPosition(const Position& position) {
    position_ = position;
}

void Piece::setState(PieceState state) {
    state_ = state;
}

bool Piece::isAirborne() const {
    return state_ == PieceState::Airborne;
}

}  // namespace kungfu


// === Source: src/pieces/King.cpp ===


namespace kungfu {

class King : public Piece {
public:
    King(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu


namespace kungfu {

King::King(PlayerColor color, Position position)
    : Piece(PieceType::King, color, std::move(position)) {}

bool King::isMovable() const {
    return true;
}

}  // namespace kungfu


// === Source: src/pieces/Queen.cpp ===


namespace kungfu {

class Queen : public Piece {
public:
    Queen(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu


namespace kungfu {

Queen::Queen(PlayerColor color, Position position)
    : Piece(PieceType::Queen, color, std::move(position)) {}

bool Queen::isMovable() const {
    return true;
}

}  // namespace kungfu


// === Source: src/pieces/Rook.cpp ===


namespace kungfu {

class Rook : public Piece {
public:
    Rook(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu


namespace kungfu {

Rook::Rook(PlayerColor color, Position position)
    : Piece(PieceType::Rook, color, std::move(position)) {}

bool Rook::isMovable() const {
    return true;
}

}  // namespace kungfu


// === Source: src/pieces/Bishop.cpp ===


namespace kungfu {

class Bishop : public Piece {
public:
    Bishop(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu


namespace kungfu {

Bishop::Bishop(PlayerColor color, Position position)
    : Piece(PieceType::Bishop, color, std::move(position)) {}

bool Bishop::isMovable() const {
    return true;
}

}  // namespace kungfu


// === Source: src/pieces/Knight.cpp ===


namespace kungfu {

class Knight : public Piece {
public:
    Knight(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu


namespace kungfu {

Knight::Knight(PlayerColor color, Position position)
    : Piece(PieceType::Knight, color, std::move(position)) {}

bool Knight::isMovable() const {
    return true;
}

}  // namespace kungfu


// === Source: src/pieces/Pawn.cpp ===


namespace kungfu {

class Pawn : public Piece {
public:
    Pawn(PlayerColor color, Position position);

    bool isMovable() const override;
};

}  // namespace kungfu


namespace kungfu {

Pawn::Pawn(PlayerColor color, Position position)
    : Piece(PieceType::Pawn, color, std::move(position)) {}

bool Pawn::isMovable() const {
    return true;
}

}  // namespace kungfu


// === Source: src/board/Board.cpp ===



namespace kungfu {

class IBoard {
public:
    virtual ~IBoard() = default;

    virtual std::optional<PiecePtr> pieceAt(const Position& position) const = 0;
    virtual bool placePiece(const PiecePtr& piece, const Position& position) = 0;
    virtual bool removePiece(const Position& position) = 0;
    virtual bool movePiece(const Position& from, const Position& to) = 0;

    // Replaces the piece at 'position' with 'newPiece' (used for pawn promotion).
    virtual bool replacePiece(const Position& position, const PiecePtr& newPiece) = 0;
};

using BoardPtr = std::shared_ptr<IBoard>;

}  // namespace kungfu

namespace kungfu {

class Board : public IBoard {
public:
    Board();
    Board(int rows, int cols); // בנאי חדש לתמיכה בממדים דינמיים

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    std::optional<PiecePtr> pieceAt(const Position& position) const override;
    bool placePiece(const PiecePtr& piece, const Position& position) override;
    bool removePiece(const Position& position) override;
    bool movePiece(const Position& from, const Position& to) override;
    bool replacePiece(const Position& position, const PiecePtr& newPiece) override;

    std::vector<PiecePtr> pieces() const;

private:
    std::vector<PiecePtr> pieces_;
    int rows_ = 8; // ברירת מחדל 8
    int cols_ = 8; // ברירת מחדל 8
};

}  // namespace kungfu

namespace kungfu {

Board::Board() : rows_(8), cols_(8) {}

Board::Board(int rows, int cols) : rows_(rows), cols_(cols) {}


std::optional<PiecePtr> Board::pieceAt(const Position& position) const {
    for (const auto& piece : pieces_) {
        if (piece && piece->position() == position) {
            return piece;
        }
    }
    return std::nullopt;
}

bool Board::placePiece(const PiecePtr& piece, const Position& position) {
    if (!piece) {
        return false;
    }

    if (pieceAt(position).has_value()) {
        return false;
    }

    piece->setPosition(position);
    pieces_.push_back(piece);
    return true;
}

bool Board::removePiece(const Position& position) {
    auto it = std::find_if(pieces_.begin(), pieces_.end(), [&](const PiecePtr& piece) {
        return piece && piece->position() == position;
    });

    if (it == pieces_.end()) {
        return false;
    }

    pieces_.erase(it);
    return true;
}

bool Board::movePiece(const Position& from, const Position& to) {
    PiecePtr movingPiece = nullptr;
    for (auto& piece : pieces_) {
        if (piece && piece->position() == from) {
            movingPiece = piece;
            break;
        }
    }

    if (!movingPiece) {
        return false;
    }

    if (pieceAt(to).has_value()) {
        removePiece(to); // הסרת הכלי הנאכל בטוחה כעת
    }

    movingPiece->setPosition(to);
    return true;
}

bool Board::replacePiece(const Position& position, const PiecePtr& newPiece) {
    auto it = std::find_if(pieces_.begin(), pieces_.end(), [&](const PiecePtr& p) {
        return p && p->position() == position;
    });

    if (it == pieces_.end() || !newPiece) {
        return false;
    }

    newPiece->setPosition(position);
    *it = newPiece;
    return true;
}

std::vector<PiecePtr> Board::pieces() const {
    return pieces_;
}

}  // namespace kungfu


// === Source: src/movement/MovementSystem.cpp ===


namespace kungfu {

class MovementSystem {
public:
    bool isInBounds(const Position& position) const;
    bool isInBounds(const Position& position, int rows, int cols) const; // פונקציה חדשה
    bool isSamePosition(const Position& from, const Position& to) const;
    bool canMoveTo(const Position& from, const Position& to) const;
    bool isValidMove(const Piece& piece, const Position& from, const Position& to) const;

    std::optional<Position> pawnDoubleStepMiddle(const Position& from, const Position& to) const;
};

}  // namespace kungfu


namespace kungfu {

struct GameConfig {
    static constexpr int kBoardSize = 8;
    static constexpr int kDefaultMoveStep = 1;

    // Pawn start rows (the row a pawn sits on at game start).
    static constexpr int kWhitePawnStartRow = 1;
    static constexpr int kBlackPawnStartRow = 6;

    // Promotion rows (the row a pawn reaches to become a queen).
    static constexpr int kWhitePawnPromotionRow = 7;
    static constexpr int kBlackPawnPromotionRow = 0;
};

}  // namespace kungfu

namespace kungfu {

bool MovementSystem::isInBounds(const Position& position) const {
    return position.row() >= 0 && position.row() < GameConfig::kBoardSize &&
           position.col() >= 0 && position.col() < GameConfig::kBoardSize;
}

bool MovementSystem::isInBounds(const Position& position, int rows, int cols) const {
    return position.row() >= 0 && position.row() < rows &&
           position.col() >= 0 && position.col() < cols;
}


bool MovementSystem::isSamePosition(const Position& from, const Position& to) const {
    return from == to;
}

bool MovementSystem::canMoveTo(const Position& from, const Position& to) const {
    return isInBounds(from) && isInBounds(to) && !isSamePosition(from, to);
}

bool MovementSystem::isValidMove(const Piece& piece, const Position& from, const Position& to) const {
    if (!canMoveTo(from, to)) {
        return false;
    }

    const int rowDelta = std::abs(to.row() - from.row());
    const int colDelta = std::abs(to.col() - from.col());

    switch (piece.type()) {
        case PieceType::King:
            return (rowDelta <= 1 && colDelta <= 1);
        case PieceType::Queen:
            return (rowDelta == 0 || colDelta == 0 || rowDelta == colDelta);
        case PieceType::Rook:
            return (rowDelta == 0 || colDelta == 0);
        case PieceType::Bishop:
            return (rowDelta == colDelta);
        case PieceType::Knight:
            return (rowDelta == 2 && colDelta == 1) || (rowDelta == 1 && colDelta == 2);
        case PieceType::Pawn: {
            if (piece.color() == PlayerColor::White) {
                const bool oneStep = (to.row() == from.row() + 1 && colDelta == 0);
                const bool twoStep = (from.row() == GameConfig::kWhitePawnStartRow &&
                                      to.row() == from.row() + 2 && colDelta == 0);
                return oneStep || twoStep;
            } else {
                const bool oneStep = (to.row() == from.row() - 1 && colDelta == 0);
                const bool twoStep = (from.row() == GameConfig::kBlackPawnStartRow &&
                                      to.row() == from.row() - 2 && colDelta == 0);
                return oneStep || twoStep;
            }
        }
    }

    return false;
}

std::optional<Position> MovementSystem::pawnDoubleStepMiddle(const Position& from, const Position& to) const {
    const int rowDelta = to.row() - from.row();
    if (std::abs(rowDelta) == 2 && from.col() == to.col()) {
        return Position(from.row() + rowDelta / 2, from.col());
    }
    return std::nullopt;
}

}  // namespace kungfu


// === Source: src/collision/CollisionSystem.cpp ===



namespace kungfu {

class ICollisionSystem {
public:
    virtual ~ICollisionSystem() = default;

    // Returns the piece at 'to' if one exists, regardless of color.
    virtual std::optional<PiecePtr> findCollision(const Position& from, const Position& to) const = 0;

    // Returns true when 'to' is occupied by an enemy piece (capture is allowed).
    virtual bool isCapture(const Position& from, const Position& to) const = 0;

    // Returns true when 'to' is occupied by a friendly piece (move must be blocked).
    virtual bool isFriendlyBlock(const Position& from, const Position& to) const = 0;

    // Returns true when all squares between 'from' and 'to' (exclusive) are empty.
    virtual bool isPathClear(const Position& from, const Position& to) const = 0;
};

using CollisionSystemPtr = std::shared_ptr<ICollisionSystem>;

}  // namespace kungfu

namespace kungfu {

class CollisionSystem : public ICollisionSystem {
public:
    explicit CollisionSystem(BoardPtr board);

    std::optional<PiecePtr> findCollision(const Position& from, const Position& to) const override;
    bool isCapture(const Position& from, const Position& to) const override;
    bool isFriendlyBlock(const Position& from, const Position& to) const override;
    bool isPathClear(const Position& from, const Position& to) const override;

private:
    BoardPtr board_;
};

}  // namespace kungfu

namespace kungfu {

CollisionSystem::CollisionSystem(BoardPtr board) : board_(std::move(board)) {}

std::optional<PiecePtr> CollisionSystem::findCollision(const Position& from, const Position& to) const {
    if (!board_) {
        return std::nullopt;
    }
    return board_->pieceAt(to);
}

bool CollisionSystem::isCapture(const Position& from, const Position& to) const {
    if (!board_) {
        return false;
    }
    const auto sourcePiece = board_->pieceAt(from);
    const auto targetPiece = board_->pieceAt(to);

    if (!sourcePiece.has_value() || !targetPiece.has_value()) {
        return false;
    }
    return sourcePiece.value()->color() != targetPiece.value()->color();
}

bool CollisionSystem::isFriendlyBlock(const Position& from, const Position& to) const {
    if (!board_) {
        return false;
    }
    const auto sourcePiece = board_->pieceAt(from);
    const auto targetPiece = board_->pieceAt(to);

    if (!sourcePiece.has_value() || !targetPiece.has_value()) {
        return false;
    }
    return sourcePiece.value()->color() == targetPiece.value()->color();
}

bool CollisionSystem::isPathClear(const Position& from, const Position& to) const {
    if (!board_) {
        return false;
    }
    const int rowDelta = to.row() - from.row();
    const int colDelta = to.col() - from.col();
    const int rowStep = (rowDelta == 0) ? 0 : (rowDelta > 0 ? 1 : -1);
    const int colStep = (colDelta == 0) ? 0 : (colDelta > 0 ? 1 : -1);

    int r = from.row() + rowStep;
    int c = from.col() + colStep;
    while (r != to.row() || c != to.col()) {
        if (board_->pieceAt(Position(r, c)).has_value()) {
            return false;
        }
        r += rowStep;
        c += colStep;
    }
    return true;
}

}  // namespace kungfu


// === Source: src/rules/RuleEngine.cpp ===



namespace kungfu {

class IRuleEngine {
public:
    virtual ~IRuleEngine() = default;
    virtual bool isValidMove(const Position& from, const Position& to) const = 0;

    // Returns true if a pawn at 'to' with the given color should be promoted.
    virtual bool isPawnPromotion(const Position& to, PlayerColor color) const = 0;
};

}  // namespace kungfu

namespace kungfu {

class RuleEngine : public IRuleEngine {
public:
    explicit RuleEngine(BoardPtr board);

    bool isValidMove(const Position& from, const Position& to) const override;
    bool isPawnPromotion(const Position& to, PlayerColor color) const override;

private:
    BoardPtr board_;
    MovementSystem movementSystem_;
};

using RuleEnginePtr = std::shared_ptr<RuleEngine>;

}  // namespace kungfu

namespace kungfu {

RuleEngine::RuleEngine(BoardPtr board) : board_(std::move(board)) {}

bool RuleEngine::isValidMove(const Position& from, const Position& to) const {
    if (!board_) {
        return false;
    }

    const auto sourcePiece = board_->pieceAt(from);
    if (!sourcePiece.has_value() || !sourcePiece.value() || !sourcePiece.value()->isMovable()) {
        return false;
    }

    // --- חוקי תנועה מיוחדים עבור רגלי (Pawn) ---
    if (sourcePiece.value()->type() == PieceType::Pawn) {
        const auto targetPiece = board_->pieceAt(to);
        const int rowDelta = to.row() - from.row();
        const int colDelta = std::abs(to.col() - from.col());
        const auto color = sourcePiece.value()->color();

        if (color == PlayerColor::White) {
            // תנועה ישרה קדימה - אסור לאכול קדימה!
            if (colDelta == 0) {
                if (targetPiece.has_value()) {
                    return false; // הדרך חסומה, רגלי לא יכול לנוע או לאכול ישר קדימה
                }
                const bool oneStep = (rowDelta == 1);
                const bool twoStep = (from.row() == GameConfig::kWhitePawnStartRow && rowDelta == 2);
                return oneStep || twoStep;
            }
            // אכילה באלכסון - חייב להיות כלי עוין ביעד כדי לאפשר את המהלך
            if (colDelta == 1 && rowDelta == 1) {
                return targetPiece.has_value() && targetPiece.value()->color() != color;
            }
        } else {
            // תנועה ישרה קדימה - אסור לאכול קדימה!
            if (colDelta == 0) {
                if (targetPiece.has_value()) {
                    return false; // הדרך חסומה, רגלי לא יכול לנוע או לאכול ישר קדימה
                }
                const bool oneStep = (rowDelta == -1);
                const bool twoStep = (from.row() == GameConfig::kBlackPawnStartRow && rowDelta == -2);
                return oneStep || twoStep;
            }
            // אכילה באלכסון - חייב להיות כלי עוין ביעד כדי לאפשר את המהלך
            if (colDelta == 1 && rowDelta == -1) {
                return targetPiece.has_value() && targetPiece.value()->color() != color;
            }
        }
        return false; // כל תנועה אחרת של רגלי אינה חוקית
    }

    // עבור שאר הכלים (מלך, מלכה, צריח, רץ ופרש) - נשענים על הגיאומטריה הרגילה
    return movementSystem_.isValidMove(*sourcePiece.value(), from, to);
}

bool RuleEngine::isPawnPromotion(const Position& to, PlayerColor color) const {
    return color == PlayerColor::White
               ? to.row() == GameConfig::kWhitePawnPromotionRow
               : to.row() == GameConfig::kBlackPawnPromotionRow;
}

}  // namespace kungfu


// === Source: src/game/Game.cpp ===


namespace kungfu {

enum class GameState {
    NotStarted,
    Running,
    Paused,
    Finished,
    Check,
    Checkmate
};

}  // namespace kungfu

namespace kungfu {

// מבנה פנימי לניהול תנועות מושהות בזמן אמת
struct PendingMove {
    Position from;
    Position to;
    int arrivalTimeMs;
};

class Game {
public:
    Game();
    explicit Game(BoardPtr board);

    void start();
    void stop();
    bool isRunning() const;
    bool isFinished() const;

    // תנועה מיידית (עבור מנוע הליבה ובדיקות יחידה)
    bool tryMove(const Position& from, const Position& to);

    // --- דרישות שלב ב' (Iteration 2) ---
    void click(int x, int y);
    void wait(int ms);
    void printBoard(std::ostream& out) const;

    // --- דרישות שלב ג' והלאה (Airborne / Jumps) ---
    bool tryJump(const Position& cell);
    void resolveJump(const Position& cell);
    bool handleArrivalAtAirbornCell(const Position& cell, const Position& arrivingFrom);

private:
    std::string getPieceToken(const PiecePtr& piece) const;

    GameState state_;
    BoardPtr board_;
    std::shared_ptr<RuleEngine> ruleEngine_;
    std::shared_ptr<CollisionSystem> collisionSystem_;
    MovementSystem movementSystem_;

    // משתני ניהול זמן וממשק אינטראקטיבי
    std::optional<Position> selectedPosition_;
    std::optional<PendingMove> pendingMove_;
    int currentTimeMs_ = 0;
};

}  // namespace kungfu

namespace kungfu {

Game::Game() : Game(std::make_shared<Board>()) {}

Game::Game(BoardPtr board) : state_(GameState::NotStarted), board_(std::move(board)), ruleEngine_(std::make_shared<RuleEngine>(board_)), collisionSystem_(std::make_shared<CollisionSystem>(board_)) {}

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



// ============================================================================
// PLATFORM HARNESS DRIVER (AUTOMATICALLY APPENDED BY BUILD_SINGLE_CPP.PY)
// ============================================================================

namespace {

bool is_valid_piece_token(const std::string& token) {
    static const std::vector<std::string> valid = {
        ".", "wK", "wQ", "wR", "wB", "wN", "wP",
        "bK", "bQ", "bR", "bB", "bN", "bP"
    };
    return std::find(valid.begin(), valid.end(), token) != valid.end();
}

std::vector<std::string> split_ws(const std::string& text) {
    std::istringstream ss(text);
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

kungfu::PiecePtr createPieceFromToken(const std::string& token, const kungfu::Position& pos) {
    if (token == "." || token.size() < 2) return nullptr;
    kungfu::PlayerColor color = (token[0] == 'w') ? kungfu::PlayerColor::White : kungfu::PlayerColor::Black;
    char type = token[1];
    switch (type) {
        case 'K': return std::make_shared<kungfu::King>(color, pos);
        case 'Q': return std::make_shared<kungfu::Queen>(color, pos);
        case 'R': return std::make_shared<kungfu::Rook>(color, pos);
        case 'B': return std::make_shared<kungfu::Bishop>(color, pos);
        case 'N': return std::make_shared<kungfu::Knight>(color, pos);
        case 'P': return std::make_shared<kungfu::Pawn>(color, pos);
        default: return nullptr;
    }
}

struct ActiveJump {
    kungfu::Position cell;
    int landTimeMs;
};

} // namespace

int main() {
    std::vector<std::string> all_lines;
    std::string line;
    bool has_sections = false;

    // קריאת כל קלט השורות
    while (std::getline(std::cin, line)) {
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), trimmed.end());

        if (trimmed == "Board:" || trimmed == "Commands:") {
            has_sections = true;
        }
        all_lines.push_back(trimmed);
    }

    if (!has_sections) {
        // --- תרחיש א': שלב א' בלבד (לוח בלבד ללא פקודות) ---
        std::vector<std::vector<std::string>> parsed_board;
        bool valid = true;
        size_t col_count = 0;

        for (const auto& row_line : all_lines) {
            std::vector<std::string> tokens = split_ws(row_line);
            if (tokens.empty()) {
                continue;
            }

            if (parsed_board.empty()) {
                col_count = tokens.size();
                if (col_count == 0) {
                    valid = false;
                }
            } else {
                if (tokens.size() != col_count) {
                    valid = false;
                }
            }

            for (const auto& token : tokens) {
                if (!is_valid_piece_token(token)) {
                    valid = false;
                }
            }
            parsed_board.push_back(tokens);
        }

        if (parsed_board.empty() || !valid) {
            std::cout << "Invalid board" << std::endl;
            return 1;
        }

        size_t rows = parsed_board.size();
        size_t cols = col_count;
        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < cols; ++c) {
                std::cout << parsed_board[r][c] << (c + 1 < cols ? " " : "");
            }
            std::cout << "\n";
        }
        return 0;
    }

    // --- תרחיש ב': משחק מלא עם כותרות ופקודות ---
    std::vector<std::vector<std::string>> board_grid;
    std::vector<std::string> command_lines;
    bool reading_board = false;
    bool reading_commands = false;

    for (const auto& trimmed : all_lines) {
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed == "Board:") {
            reading_board = true;
            reading_commands = false;
            continue;
        }
        if (trimmed == "Commands:") {
            reading_board = false;
            reading_commands = true;
            continue;
        }
        if (reading_board) {
            board_grid.push_back(split_ws(trimmed));
        } else if (reading_commands) {
            command_lines.push_back(trimmed);
        }
    }

    // יצירת לוח מודולרי מהמחלקה האמיתית שלך בממדים שהוסקו דינמית
    int rows = board_grid.size();
    int cols = (rows > 0) ? board_grid[0].size() : 0;
    auto board = std::make_shared<kungfu::Board>(rows, cols);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto piece = createPieceFromToken(board_grid[r][c], kungfu::Position(r, c));
            if (piece) {
                board->placePiece(piece, kungfu::Position(r, c));
            }
        }
    }

    // הרצת המשחק
    kungfu::Game game(board);
    game.start();

    int currentTimeMs = 0;
    std::vector<ActiveJump> activeJumps;

    for (const std::string& command : command_lines) {
        std::istringstream ss(command);
        std::string verb;
        ss >> verb;
        if (verb == "click") {
            int x = 0;
            int y = 0;
            ss >> x >> y;
            game.click(x, y);
        } else if (verb == "jump") {
            int x = 0;
            int y = 0;
            ss >> x >> y;
            kungfu::Position pos{y / 100, x / 100};
            if (game.tryJump(pos)) {
                activeJumps.push_back(ActiveJump{pos, currentTimeMs + 1000});
            }
        } else if (verb == "wait") {
            int ms = 0;
            ss >> ms;
            currentTimeMs += ms;
            
            // הרצת שעון המשחק ופתרון תנועות רגילות
            game.wait(ms);
            
            // פתרון קפיצות שהגיע זמן נחיתתן (1,000 מילישניות מתחילת הקפיצה)
            for (auto it = activeJumps.begin(); it != activeJumps.end(); ) {
                if (currentTimeMs >= it->landTimeMs) {
                    game.resolveJump(it->cell);
                    it = activeJumps.erase(it); // הסרה בטוחה מתוך לולאה
                } else {
                    ++it;
                }
            }
        } else if (verb == "print") {
            std::string target;
            ss >> target;
            if (target == "board") {
                game.printBoard(std::cout);
            }
        }
    }

    return 0;
}
