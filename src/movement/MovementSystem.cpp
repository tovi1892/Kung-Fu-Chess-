#include "movement/MovementSystem.hpp"
#include <cmath>
#include "common/GameConfig.hpp"

namespace kungfu {

bool MovementSystem::isInBounds(const Position& position) const {
    return position.row() >= 0 && position.row() < GameConfig::kBoardSize &&
           position.col() >= 0 && position.col() < GameConfig::kBoardSize;
}

bool MovementSystem::isInBounds(const Position& position, int rows, int cols) const {
    return position.row() >= 0 && position.row() < rows &&
           position.col() >= 0 && position.col() < cols;
}


bool MovementSystem::isSamePosition(const Position& from, const Position& to) const {
    return from == to;
}

bool MovementSystem::canMoveTo(const Position& from, const Position& to) const {
    return isInBounds(from) && isInBounds(to) && !isSamePosition(from, to);
}

bool MovementSystem::isValidMove(const Piece& piece, const Position& from, const Position& to) const {
    if (!canMoveTo(from, to)) {
        return false;
    }

    const int rowDelta = std::abs(to.row() - from.row());
    const int colDelta = std::abs(to.col() - from.col());

    switch (piece.type()) {
        case PieceType::King:
            return (rowDelta <= 1 && colDelta <= 1);
        case PieceType::Queen:
            return (rowDelta == 0 || colDelta == 0 || rowDelta == colDelta);
        case PieceType::Rook:
            return (rowDelta == 0 || colDelta == 0);
        case PieceType::Bishop:
            return (rowDelta == colDelta);
        case PieceType::Knight:
            return (rowDelta == 2 && colDelta == 1) || (rowDelta == 1 && colDelta == 2);
        case PieceType::Pawn: {
            if (piece.color() == PlayerColor::White) {
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
    }

    return false;
}

std::optional<Position> MovementSystem::pawnDoubleStepMiddle(const Position& from, const Position& to) const {
    const int rowDelta = to.row() - from.row();
    if (std::abs(rowDelta) == 2 && from.col() == to.col()) {
        return Position(from.row() + rowDelta / 2, from.col());
    }
    return std::nullopt;
}

}  // namespace kungfu
