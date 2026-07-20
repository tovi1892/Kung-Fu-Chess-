#include "io/BoardPrinter.hpp"

#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

std::string BoardPrinter::pieceToken(const Piece* piece) {
    if (!piece) return ".";
    std::string token = (piece->color() == PlayerColor::White) ? "w" : "b";
     token += pieceTypeChar(piece->type());
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
