#include "rules/RuleEngine.hpp"

#include "model/GameConfig.hpp"
#include "rules/PieceRules.hpp"

namespace kungfu {
namespace {

bool isOutsideBoard(const Position& position, int rows, int cols) {
    return position.row() < 0 || position.row() >= rows ||
           position.col() < 0 || position.col() >= cols;
}

}  // namespace

RuleEngine::RuleEngine(BoardPtr board) : board_(std::move(board)) {}

MoveValidation RuleEngine::validateMove(const Position& from, const Position& to) const {
    if (!board_) {
        return {false, "illegal_piece_move"};
    }

    if (isOutsideBoard(from, board_->rows(), board_->cols()) ||
        isOutsideBoard(to, board_->rows(), board_->cols())) {
        return {false, "outside_board"};
    }

    const auto sourcePiece = board_->pieceAt(from);
    if (!sourcePiece.has_value() || !sourcePiece.value()) {
        return {false, "empty_source"};
    }
    const Piece* movingPiece = sourcePiece.value();

    // A friendly piece occupies 'to' and blocks it - unless that piece is
    // itself already Moving (on its way out), in which case its square is
    // fair game the instant it starts leaving, not only once it finishes
    // crossing into the next cell. Mirrors PieceRules' own exception, so
    // this early-exit reason stays consistent with legalDestinations below.
    const auto targetPiece = board_->pieceAt(to);
    if (targetPiece.has_value() && targetPiece.value() != nullptr &&
        targetPiece.value()->color() == movingPiece->color() &&
        targetPiece.value()->state() != PieceState::Moving) {
        return {false, "friendly_destination"};
    }

    const auto legalDestinations = pieceRulesFor(movingPiece->type()).legalDestinations(*board_, *movingPiece);
    return legalDestinations.count(to) > 0
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
