#pragma once

#include <memory>
#include <optional>
#include "common/Position.hpp"
#include "pieces/Piece.hpp"

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
