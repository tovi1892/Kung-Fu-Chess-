#include "input/Controller.hpp"

#include <utility>
#include "model/GameConfig.hpp"

namespace kungfu {

namespace {

// A cell is selectable when it holds a piece that isn't already mid-motion.
bool isSelectable(const std::shared_ptr<GameEngine>& game, const Position& pos) {
    const auto piece = game->getBoard()->pieceAt(pos);
    return piece.has_value() && piece.value() != nullptr &&
           piece.value()->state() != PieceState::Moving;
}

int clampToBoard(int value, int maxBound) {
    if (value < 0) {
        return 0;
    }
    if (value >= maxBound) {
        return maxBound - 1;
    }
    return value;
}

}  // namespace

Controller::Controller(std::shared_ptr<GameEngine> game) : game_(std::move(game)) {}

bool Controller::handleClick(int x, int y) {
    if (!game_ || !game_->isRunning()) {
        return false;
    }

    const int row = clampToBoard(y / GameConfig::kCellSizePx, boardRows());
    const int col = clampToBoard(x / GameConfig::kCellSizePx, boardCols());
    return handleCellClick(row, col);
}

void Controller::attachGame(std::shared_ptr<GameEngine> game) {
    game_ = std::move(game);
    selectedPosition_.reset();
}

std::shared_ptr<GameEngine> Controller::game() const {
    return game_;
}

bool Controller::handleCellClick(int row, int col) {
    if (!game_ || !game_->isRunning()) {
        return false;
    }

    const Position clicked(row, col);
    if (!game_->isPositionInBounds(clicked)) {
        if (selectedPosition_.has_value()) {
            selectedPosition_.reset();
        }
        return false;
    }

    const bool hasSelection = selectedPosition_.has_value();

    if (!hasSelection) {
        if (!isSelectable(game_, clicked)) {
            return false;
        }
        selectedPosition_ = clicked;
        return true;
    }

    const Position from = *selectedPosition_;
    if (clicked == from) {
        selectedPosition_.reset();
        return game_->requestJump(clicked);
    }

    if (isFriendlyPieceAt(clicked) && isSelectable(game_, clicked)) {
        selectedPosition_ = clicked;
        return true;
    }

    const auto result = game_->requestMove(from, clicked);
    if (result.is_accepted) {
        selectedPosition_.reset();
    }
    return result.is_accepted;
}

void Controller::handleTimePassed(int ms) {
    if (!game_) {
        return;
    }
    game_->wait(ms);
}

bool Controller::isRunning() const {
    return game_ ? game_->isRunning() : false;
}

bool Controller::isFriendlyPieceAt(const Position& pos) const {
    if (!game_ || !selectedPosition_.has_value()) {
        return false;
    }

    const auto selectedPiece = game_->getBoard()->pieceAt(*selectedPosition_);
    const auto targetPiece = game_->getBoard()->pieceAt(pos);
    return selectedPiece.has_value() && targetPiece.has_value() &&
           selectedPiece.value() && targetPiece.value() &&
           selectedPiece.value()->color() == targetPiece.value()->color();
}

bool Controller::selectPiece(const Position& pos) {
    if (!game_) {
        return false;
    }

    if (!selectedPosition_.has_value()) {
        if (!isSelectable(game_, pos)) {
            return false;
        }
        selectedPosition_ = pos;
        return true;
    }

    if (pos == *selectedPosition_) {
        selectedPosition_.reset();
        return game_->requestJump(pos);
    }

    if (isFriendlyPieceAt(pos) && isSelectable(game_, pos)) {
        selectedPosition_ = pos;
        return true;
    }

    const auto result = game_->requestMove(*selectedPosition_, pos);
    if (result.is_accepted) {
        selectedPosition_.reset();
    }
    return result.is_accepted;
}

bool Controller::requestMove(const Position& from, const Position& to) {
    if (!game_) {
        return false;
    }

    const auto result = game_->requestMove(from, to);
    if (result.is_accepted) {
        selectedPosition_.reset();
    }
    return result.is_accepted;
}

bool Controller::requestJump(const Position& pos) {
    if (!game_) {
        return false;
    }

    const bool jumped = game_->requestJump(pos);
    if (jumped) {
        selectedPosition_.reset();
    }
    return jumped;
}

bool Controller::hasSelection() const {
    return selectedPosition_.has_value();
}

std::optional<Position> Controller::selectedPosition() const {
    return selectedPosition_;
}

bool Controller::isPositionInBounds(const Position& pos) const {
    return game_ ? game_->isPositionInBounds(pos) : false;
}

int Controller::boardRows() const {
    return game_ ? game_->boardRows() : 0;
}

int Controller::boardCols() const {
    return game_ ? game_->boardCols() : 0;
}

}  // namespace kungfu
