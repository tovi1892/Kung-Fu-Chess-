#include "history/GameRecord.hpp"

#include "model/GameConfig.hpp"

namespace kungfu {

namespace {

char pieceLetter(PieceType type) {
    // Chess notation omits the pawn's letter entirely - every other type
    // uses the shared canonical code.
    return type == PieceType::Pawn ? '\0' : pieceTypeChar(type);
}

std::string squareName(const Position& pos) {
    const char file = static_cast<char>('a' + pos.col());
    const int rank = GameConfig::kBoardSize - pos.row();
    return std::string(1, file) + std::to_string(rank);
}

}  // namespace

int pieceValue(PieceType type) {
    switch (type) {
        case PieceType::Knight:
        case PieceType::Bishop: return 3;
        case PieceType::Rook:   return 5;
        case PieceType::Queen:  return 9;
        case PieceType::Pawn:   return 1;
        case PieceType::King:   return 0;
    }
    return 0;
}

std::string moveNotation(PieceType type, const Position& from, const Position& to, bool isCapture) {
    const char letter = pieceLetter(type);

    std::string out;
    if (letter != '\0') {
        out += letter;
    } else if (isCapture) {
        // A pawn capture shows its origin file instead of a piece letter (e.g. "exd5").
        out += static_cast<char>('a' + from.col());
    }
    if (isCapture) {
        out += 'x';
    }
    out += squareName(to);
    return out;
}

void GameRecord::recordMove(PlayerColor color, int elapsedMs, const std::string& notation) {
    auto& moves = (color == PlayerColor::White) ? whiteMoves_ : blackMoves_;
    moves.push_back({elapsedMs, notation});
}

void GameRecord::recordCapture(PlayerColor capturingColor, PieceType capturedType) {
    int& score = (capturingColor == PlayerColor::White) ? whiteScore_ : blackScore_;
    score += pieceValue(capturedType);
}

const std::vector<MoveRecord>& GameRecord::movesFor(PlayerColor color) const {
    return color == PlayerColor::White ? whiteMoves_ : blackMoves_;
}

int GameRecord::scoreFor(PlayerColor color) const {
    return color == PlayerColor::White ? whiteScore_ : blackScore_;
}

}  // namespace kungfu
