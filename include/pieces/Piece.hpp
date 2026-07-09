#pragma once

#include <memory>
#include "common/Enums.hpp"
#include "common/Position.hpp"

namespace kungfu {

class Piece {
public:
    Piece(PieceType type, PlayerColor color, Position position);
    virtual ~Piece() = default;

    PieceType type() const;
    PlayerColor color() const;
    Position position() const;
    PieceState state() const;

    void setPosition(const Position& position);
    void setState(PieceState state);

    bool isAirborne() const;

    virtual bool isMovable() const = 0;

protected:
    PieceType type_;
    PlayerColor color_;
    Position position_;
    PieceState state_ = PieceState::Idle;
};

using PiecePtr = std::shared_ptr<Piece>;

}  // namespace kungfu
