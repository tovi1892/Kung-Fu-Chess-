#pragma once

#include <memory>
#include <optional>
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

class IBoard {
public:
    virtual ~IBoard() = default;

    virtual int rows() const = 0;
    virtual int cols() const = 0;

    virtual std::optional<Piece*> pieceAt(const Position& position) const = 0;
    virtual bool placePiece(std::unique_ptr<Piece> piece, const Position& position) = 0;
    virtual bool removePiece(const Position& position) = 0;
    virtual bool movePiece(const Position& from, const Position& to) = 0;

    // Replaces the piece at 'position' with 'newPiece' (used for pawn promotion).
    virtual bool replacePiece(const Position& position, std::unique_ptr<Piece> newPiece) = 0;
};

using BoardPtr = std::shared_ptr<IBoard>;

}  // namespace kungfu
