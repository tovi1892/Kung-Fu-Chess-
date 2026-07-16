#pragma once

#include <memory>
#include <optional>
#include "model/IBoard.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

// True when every square strictly between 'from' and 'to' (exclusive of both
// endpoints) is empty. Shared geometry: used by PieceRules' sliding-piece
// classes for static legality, and independently by CollisionSystem below.
bool isPathClear(const IBoard& board, const Position& from, const Position& to);

// Static (non-real-time) occupancy queries for a single move, built on top
// of isPathClear above.
class ICollisionSystem {
public:
    virtual ~ICollisionSystem() = default;

    // The piece at 'to', if any, regardless of color.
    virtual std::optional<Piece*> findCollision(const Position& from, const Position& to) const = 0;

    // True when 'to' is occupied by an enemy piece (capture is allowed).
    virtual bool isCapture(const Position& from, const Position& to) const = 0;

    // True when 'to' is occupied by a friendly piece (move must be blocked).
    virtual bool isFriendlyBlock(const Position& from, const Position& to) const = 0;

    // True when every square between 'from' and 'to' (exclusive) is empty.
    virtual bool isPathClear(const Position& from, const Position& to) const = 0;
};

using CollisionSystemPtr = std::shared_ptr<ICollisionSystem>;

}  // namespace kungfu
