#include "model/pieces/Pawn.hpp"

#include "model/GameConfig.hpp"
#include <cmath>
#include <utility>

namespace kungfu {

Pawn::Pawn(PlayerColor color, Position position)
    : Piece(PieceType::Pawn, color, std::move(position)) {}

bool Pawn::isMovable() const {
    return true;
}

bool Pawn::isMoveValid(const Position& from, const Position& to) const {
    const int colDelta = std::abs(to.col() - from.col());
    if (color() == PlayerColor::White) {
        const bool oneStep = (to.row() == from.row() + 1 && colDelta == 0);
        const bool twoStep = (from.row() == GameConfig::kWhitePawnStartRow &&
                              to.row() == from.row() + 2 && colDelta == 0);
        return oneStep || twoStep;
    } else {
        const bool oneStep = (to.row() == from.row() - 1 && colDelta == 0);
        const bool twoStep = (from.row() == GameConfig::kBlackPawnStartRow &&
                              to.row() == from.row() - 2 && colDelta == 0);
        return oneStep || twoStep;
    }
}

}  // namespace kungfu
