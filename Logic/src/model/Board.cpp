#include "model/Board.hpp"
#include "IGameView.hpp"
#include <cstdint>
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

    auto it = std::find_if(pieces_.begin(), pieces_.end(), [&](const std::unique_ptr<Piece>& p) {
        return p && p->position() == position;
    });

    newPiece->setPosition(position);
    if (it != pieces_.end()) {
        *it = std::move(newPiece);
    } else {
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

std::vector<RenderPiece> Board::getRenderState() const {
    std::vector<RenderPiece> out;
    out.reserve(pieces_.size());
    for (const auto& p : pieces_) {
        if (!p) continue;
        RenderPiece rp;
        rp.id = static_cast<uintptr_t>(p->id());
        rp.type = static_cast<int>(p->type());
        rp.color = static_cast<int>(p->color());
        rp.x = p->position().row();
        rp.y = p->position().col();
        rp.state = static_cast<int>(p->state());
        rp.cooldownMs = 0.0;       // overlaid by GameEngine::getRenderState, which has arbiter access
        rp.cooldownTotalMs = 0.0;
        out.push_back(rp);
    }
    return out;
}

}  // namespace kungfu
