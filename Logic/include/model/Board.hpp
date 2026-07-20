#pragma once

#include <memory>
#include <vector>
#include "model/IBoard.hpp"
#include "model/Enums.hpp"
#include "model/GameConfig.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

// Forward-declare render struct from Core_Interfaces to avoid direct header dependency
namespace kungfu { struct RenderPiece; }

namespace kungfu {

// The only IBoard implementation: an in-memory, owning list of pieces. See
// IBoard for what each method does - this class doesn't change those
// contracts, just implements them.
class Board : public IBoard {
public:
    // An empty 8x8 board.
    Board();

    // An empty board with a custom size (e.g. for a script-driven test board
    // parsed by BoardParser).
    Board(int rows, int cols);

    int rows() const override { return rows_; }
    int cols() const override { return cols_; }

    std::optional<Piece*> pieceAt(const Position& position) const override;
    bool placePiece(std::unique_ptr<Piece> piece, const Position& position) override;
    bool removePiece(const Position& position) override;
    bool movePiece(const Position& from, const Position& to) override;
    bool replacePiece(const Position& position, std::unique_ptr<Piece> newPiece) override;

    std::vector<Piece*> pieces() const override;

    // A lightweight render snapshot for the UI: one RenderPiece per piece,
    // built straight from stored id/type/color/position/state. Non-owning.
    std::vector<kungfu::RenderPiece> getRenderState() const;

private:
    std::vector<std::unique_ptr<Piece>> pieces_;
    int rows_ = GameConfig::kBoardSize;
    int cols_ = GameConfig::kBoardSize;
};

}  // namespace kungfu
