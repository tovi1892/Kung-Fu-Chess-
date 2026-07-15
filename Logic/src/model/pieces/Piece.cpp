#include "model/pieces/Piece.hpp"

#include <atomic>

namespace kungfu {

namespace {
std::atomic<int> nextPieceId{1};
}

Piece::Piece(PieceType type, PlayerColor color, Position position)
    : id_(nextPieceId++), type_(type), color_(color), position_(position), state_(PieceState::Idle) {}

int Piece::id() const {
    return id_;
}

PieceType Piece::type() const {
    return type_;
}

PlayerColor Piece::color() const {
    return color_;
}

Position Piece::position() const {
    return position_;
}

PieceState Piece::state() const {
    return state_;
}

void Piece::setPosition(const Position& position) {
    position_ = position;
}

void Piece::setState(PieceState state) {
    state_ = state;
}

bool Piece::isAirborne() const {
    return state_ == PieceState::Airborne;
}

}  // namespace kungfu
