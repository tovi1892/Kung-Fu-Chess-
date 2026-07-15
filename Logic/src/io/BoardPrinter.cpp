#include "io/BoardPrinter.hpp"

#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

std::string BoardPrinter::pieceToken(const Piece* piece) {
    if (!piece) return ".";
    std::string token = (piece->color() == PlayerColor::White) ? "w" : "b";
    switch (piece->type()) {
        case PieceType::King:   token += "K"; break;
        case PieceType::Queen:  token += "Q"; break;
        case PieceType::Rook:   token += "R"; break;
        case PieceType::Bishop: token += "B"; break;
        case PieceType::Knight: token += "N"; break;
        case PieceType::Pawn:   token += "P"; break;
    }
    return token;
}

void BoardPrinter::print(std::ostream& out, const IBoard& board, int rows, int cols) const {
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto piece = board.pieceAt(Position(r, c));
            if (piece.has_value()) {
                out << pieceToken(piece.value());
            } else {
                out << ".";
            }
            if (c + 1 < cols) {
                out << " ";
            }
        }
        out << "\n";
    }
}

}  // namespace kungfu
