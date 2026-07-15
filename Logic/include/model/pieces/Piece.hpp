#pragma once

#include <memory>
#include "model/Enums.hpp"
#include "model/Position.hpp"

namespace kungfu {

class Piece {
public:
    Piece(PieceType type, PlayerColor color, Position position);
    virtual ~Piece() = default;

    // Stable id assigned once at creation time (course spec section 6: "id: unique
    // stable id"). Used to correlate a piece across board snapshots and in-flight
    // motions without relying on its (potentially-moving) memory address.
    int id() const;

    PieceType type() const;
    PlayerColor color() const;
    Position position() const;
    PieceState state() const;

    void setPosition(const Position& position);
    void setState(PieceState state);

    bool isAirborne() const;

    virtual bool isMovable() const = 0;
    virtual bool isMoveValid(const Position& from, const Position& to) const = 0;

protected:
    int id_;
    PieceType type_;
    PlayerColor color_;
    Position position_;
    PieceState state_ = PieceState::Idle;
};

using PiecePtr = std::shared_ptr<Piece>;

}  // namespace kungfu
