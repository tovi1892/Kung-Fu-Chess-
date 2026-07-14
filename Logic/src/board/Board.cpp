#include "board/Board.hpp"
#include <algorithm>

namespace kungfu {

Board::Board() : rows_(8), cols_(8) {}

Board::Board(int rows, int cols) : rows_(rows), cols_(cols) {}


std::optional<Piece*> Board::pieceAt(const Position& position) const {
    for (const auto& piece : pieces_) {
        if (piece && piece->position() == position) {
            return piece.get();
        }
    }
    return std::nullopt;
}

bool Board::placePiece(std::unique_ptr<Piece> piece, const Position& position) {
    if (!piece) {
        return false;
    }

    if (this->pieceAt(position).has_value()) {
        return false;
    }

    piece->setPosition(position);
    pieces_.push_back(std::move(piece));
    return true;
}

bool Board::removePiece(const Position& position) {
    auto it = std::find_if(pieces_.begin(), pieces_.end(), [&](const std::unique_ptr<Piece>& piece) {
        return piece && piece->position() == position;
    });

    if (it == pieces_.end()) {
        return false;
    }

    pieces_.erase(it);
    return true;
}

bool Board::movePiece(const Position& from, const Position& to) {
    Piece* movingPiece = nullptr;
    for (const auto& piece : pieces_) {
        if (piece && piece->position() == from) {
            movingPiece = piece.get();
            break;
        }
    }

    if (!movingPiece) {
        return false;
    }

    if (pieceAt(to).has_value()) {
        return false;
    }

    movingPiece->setPosition(to);
    return true;
}

bool Board::replacePiece(const Position& position, std::unique_ptr<Piece> newPiece) {
    if (!newPiece) return false;

    // מחפשים אם כבר קיים כלי במיקום הזה
    auto it = std::find_if(pieces_.begin(), pieces_.end(), [&](const std::unique_ptr<Piece>& p) {
        return p && p->position() == position;
    });

    if (it != pieces_.end()) {
        // אם מצאנו כלי, מחליפים אותו
        newPiece->setPosition(position);
        *it = std::move(newPiece);
    } else {
        // אם לא מצאנו כלי, מוסיפים את הכלי החדש לרשימה
        newPiece->setPosition(position);
        pieces_.push_back(std::move(newPiece));
    }
    return true;
}
std::vector<Piece*> Board::pieces() const {
    std::vector<Piece*> result;
    result.reserve(pieces_.size());
    for (const auto& piece : pieces_) {
        if (piece) {
            result.push_back(piece.get());
        }
    }
    return result;
}

}  // namespace kungfu
