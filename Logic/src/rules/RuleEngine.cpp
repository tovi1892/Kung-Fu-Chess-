#include "rules/RuleEngine.hpp"
#include "common/GameConfig.hpp"
#include <cmath>

namespace kungfu {
namespace {

bool isOutsideBoard(const Position& position) {
    return position.row() < 0 || position.row() >= GameConfig::kBoardSize ||
           position.col() < 0 || position.col() >= GameConfig::kBoardSize;
}

}  // namespace

RuleEngine::RuleEngine(BoardPtr board) : board_(std::move(board)) {}

MoveValidation RuleEngine::validateMove(const Position& from, const Position& to) const {
    if (!board_) {
        return {false, "illegal_piece_move"};
    }

    if (isOutsideBoard(from) || isOutsideBoard(to)) {
        return {false, "outside_board"};
    }

    const auto sourcePiece = board_->pieceAt(from);
    if (!sourcePiece.has_value() || !sourcePiece.value()) {
        return {false, "empty_source"};
    }

    const auto targetPiece = board_->pieceAt(to);
    const auto movingPiece = sourcePiece.value();
    
    // הבדיקה שחסמה תנועה למשבצת עם כלי ידידותי הוסרה מכאן.

    if (!movingPiece->isMovable()) {
        return {false, "illegal_piece_move"};
    }

    // התיקון: בדיקות בטוחות של nullptr עבור הרגלי (Pawn)
    if (movingPiece->type() == PieceType::Pawn) {
        const int rowDelta = to.row() - from.row();
        const int colDelta = std::abs(to.col() - from.col());
        const auto color = movingPiece->color();

        if (color == PlayerColor::White) {
            if (colDelta == 0) {
                // וידוא שהמשבצת לא ריקה לפני חסימה
                if (targetPiece.has_value() && targetPiece.value() != nullptr) {
                    return {false, "illegal_piece_move"};
                }
                const bool oneStep = (rowDelta == 1);
                const bool twoStep = (from.row() == GameConfig::kWhitePawnStartRow && rowDelta == 2);
                return oneStep || twoStep ? MoveValidation{true, "ok"} : MoveValidation{false, "illegal_piece_move"};
            }
            if (colDelta == 1 && rowDelta == 1) {
                // וידוא שהמשבצת לא ריקה לפני בדיקת צבע באכילה
                const bool canCapture = targetPiece.has_value() && targetPiece.value() != nullptr && targetPiece.value()->color() != color;
                return canCapture ? MoveValidation{true, "ok"} : MoveValidation{false, "illegal_piece_move"};
            }
        } else {
            if (colDelta == 0) {
                // וידוא שהמשבצת לא ריקה לפני חסימה
                if (targetPiece.has_value() && targetPiece.value() != nullptr) {
                    return {false, "illegal_piece_move"};
                }
                const bool oneStep = (rowDelta == -1);
                const bool twoStep = (from.row() == GameConfig::kBlackPawnStartRow && rowDelta == -2);
                return oneStep || twoStep ? MoveValidation{true, "ok"} : MoveValidation{false, "illegal_piece_move"};
            }
            if (colDelta == 1 && rowDelta == -1) {
                // וידוא שהמשבצת לא ריקה לפני בדיקת צבע באכילה
                const bool canCapture = targetPiece.has_value() && targetPiece.value() != nullptr && targetPiece.value()->color() != color;
                return canCapture ? MoveValidation{true, "ok"} : MoveValidation{false, "illegal_piece_move"};
            }
        }
        return {false, "illegal_piece_move"};
    }

    return movementSystem_.isValidMove(*movingPiece, from, to)
               ? MoveValidation{true, "ok"}
               : MoveValidation{false, "illegal_piece_move"};
}
bool RuleEngine::isValidMove(const Position& from, const Position& to) const {
    return validateMove(from, to).is_valid;
}

bool RuleEngine::isPawnPromotion(const Position& to, PlayerColor color) const {
    return color == PlayerColor::White
               ? to.row() == GameConfig::kWhitePawnPromotionRow
               : to.row() == GameConfig::kBlackPawnPromotionRow;
}

}  // namespace kungfu