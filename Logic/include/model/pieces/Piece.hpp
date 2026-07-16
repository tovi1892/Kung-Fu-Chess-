#pragma once

#include <memory>
#include "model/Enums.hpp"
#include "model/Position.hpp"

namespace kungfu {

// A single piece on the board: its identity (type, color, stable id) and its
// current lifecycle state (position, PieceState). Carries no movement rules
// or behavior of its own - see rules/PieceRules for that.
class Piece {
public:
    Piece(PieceType type, PlayerColor color, Position position);
    virtual ~Piece() = default;

    // Stable id assigned once at creation time and never reused. Used to
    // correlate a piece across board snapshots and in-flight motions without
    // relying on its (potentially-moving) memory address.
    int id() const;

    PieceType type() const;
    PlayerColor color() const;
    Position position() const;
    PieceState state() const;

    // The only mutators - called by Board (position) and RealTimeArbiter/
    // GameEngine (state), never by rules code.
    void setPosition(const Position& position);
    void setState(PieceState state);

    // Shorthand for state() == PieceState::Airborne.
    bool isAirborne() const;

protected:
    int id_;
    PieceType type_;
    PlayerColor color_;
    Position position_;
    PieceState state_ = PieceState::Idle;
};

using PiecePtr = std::shared_ptr<Piece>;

}  // namespace kungfu
