#include "collision/CollisionSystem.hpp"

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