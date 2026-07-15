#include "game/GameController.hpp"

#include <utility>

namespace kungfu {

GameController::GameController(std::shared_ptr<Game> game) : game_(std::move(game)) {}

void GameController::attachGame(std::shared_ptr<Game> game) {
    game_ = std::move(game);
    selectedPosition_.reset();
}

std::shared_ptr<Game> GameController::game() const {
    return game_;
}

bool GameController::handleCellClick(int row, int col) {
    if (!game_ || !game_->isRunning()) {
        return false;
    }

    const Position clicked(row, col);
    if (!game_->isPositionInBounds(clicked)) {
        return false;
    }

    const auto clickedPiece = game_->getBoard()->pieceAt(clicked);
    const bool hasSelection = selectedPosition_.has_value();

    if (!hasSelection) {
        if (!clickedPiece.has_value() || !clickedPiece.value()) {
            return false;
        }

        selectedPosition_ = clicked;
        return game_->selectPiece(clicked);
    }

    const Position from = *selectedPosition_;
    if (clicked == from) {
        selectedPosition_.reset();
        return game_->requestJump(clicked);
    }

    if (clickedPiece.has_value() && clickedPiece.value() && isFriendlyPieceAt(clicked)) {
        selectedPosition_ = clicked;
        return game_->selectPiece(clicked);
    }

    const bool moved = game_->requestMove(from, clicked);
    if (moved) {
        selectedPosition_.reset();
    }
    return moved;
}

void GameController::handleTimePassed(int ms) {
    if (!game_) {
        return;
    }
    game_->wait(ms);
}

bool GameController::isRunning() const {
    return game_ ? game_->isRunning() : false;
}

bool GameController::isFriendlyPieceAt(const Position& pos) const {
    if (!game_ || !selectedPosition_.has_value()) {
        return false;
    }

    const auto selectedPiece = game_->getBoard()->pieceAt(*selectedPosition_);
    const auto targetPiece = game_->getBoard()->pieceAt(pos);
    return selectedPiece.has_value() && targetPiece.has_value() &&
           selectedPiece.value() && targetPiece.value() &&
           selectedPiece.value()->color() == targetPiece.value()->color();
}

bool GameController::selectPiece(const Position& pos) {
    if (!game_) {
        return false;
    }

    if (!selectedPosition_.has_value()) {
        selectedPosition_ = pos;
        return game_->selectPiece(pos);
    }

    if (pos == *selectedPosition_) {
        selectedPosition_.reset();
        return game_->requestJump(pos);
    }

    const auto clickedPiece = game_->getBoard()->pieceAt(pos);
    if (clickedPiece.has_value() && clickedPiece.value() && game_->isFriendlyPieceAt(pos)) {
        selectedPosition_ = pos;
        return game_->selectPiece(pos);
    }

    const bool moved = game_->requestMove(*selectedPosition_, pos);
    if (moved) {
        selectedPosition_.reset();
    }
    return moved;
}

bool GameController::requestMove(const Position& from, const Position& to) {
    if (!game_) {
        return false;
    }

    const bool moved = game_->requestMove(from, to);
    if (moved) {
        selectedPosition_.reset();
    }
    return moved;
}

bool GameController::requestJump(const Position& pos) {
    if (!game_) {
        return false;
    }

    const bool jumped = game_->requestJump(pos);
    if (jumped) {
        selectedPosition_.reset();
    }
    return jumped;
}

bool GameController::hasSelection() const {
    return selectedPosition_.has_value();
}

std::optional<Position> GameController::selectedPosition() const {
    return selectedPosition_;
}

bool GameController::isPositionInBounds(const Position& pos) const {
    return game_ ? game_->isPositionInBounds(pos) : false;
}

int GameController::boardRows() const {
    return game_ ? game_->boardRows() : 0;
}

int GameController::boardCols() const {
    return game_ ? game_->boardCols() : 0;
}

}  // namespace kungfu
