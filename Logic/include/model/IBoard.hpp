#pragma once

#include <memory>
#include <optional>
#include <vector>
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

// A board's occupancy: which squares exist and which piece, if any, sits on
// each one. Says nothing about movement legality or turn order - that's
// rules/'s job, built on top of this read/write surface.
class IBoard {
public:
    virtual ~IBoard() = default;

    // The board's dimensions (8x8 by default; can differ for a board built
    // from a text script via BoardParser).
    virtual int rows() const = 0;
    virtual int cols() const = 0;

    // The piece at 'position', or nullopt if the square is empty or out of bounds.
    virtual std::optional<Piece*> pieceAt(const Position& position) const = 0;

    // Places 'piece' at 'position', taking ownership of it. Returns false
    // (and drops 'piece') if the square is already occupied.
    virtual bool placePiece(std::unique_ptr<Piece> piece, const Position& position) = 0;

    // Removes whatever piece is at 'position'. Returns false if the square was already empty.
    virtual bool removePiece(const Position& position) = 0;

    // Moves whichever piece is at 'from' to 'to', with no legality check of
    // any kind. Returns false if there's no piece at 'from' or 'to' is
    // already occupied.
    virtual bool movePiece(const Position& from, const Position& to) = 0;

    // Replaces the piece at 'position' with 'newPiece' (used for pawn promotion).
    virtual bool replacePiece(const Position& position, std::unique_ptr<Piece> newPiece) = 0;

    // Non-owning list of every piece currently on the board.
    virtual std::vector<Piece*> pieces() const = 0;
};

using BoardPtr = std::shared_ptr<IBoard>;

}  // namespace kungfu
