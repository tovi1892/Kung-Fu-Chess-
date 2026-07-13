#pragma once

#include <memory>
#include <memory>
#include <vector>
#include "board/IBoard.hpp"
#include "common/Enums.hpp"
#include "common/Position.hpp"
#include "pieces/Piece.hpp"

namespace kungfu {

class Board : public IBoard {
public:
    Board();
    Board(int rows, int cols); // בנאי חדש לתמיכה בממדים דינמיים

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    std::optional<Piece*> pieceAt(const Position& position) const override;
    bool placePiece(std::unique_ptr<Piece> piece, const Position& position) override;
    bool removePiece(const Position& position) override;
    bool movePiece(const Position& from, const Position& to) override;
    bool replacePiece(const Position& position, std::unique_ptr<Piece> newPiece) override;

    std::vector<Piece*> pieces() const;

private:
    std::vector<std::unique_ptr<Piece>> pieces_;
    int rows_ = 8; // ברירת מחדל 8
    int cols_ = 8; // ברירת מחדל 8
};

}  // namespace kungfu
