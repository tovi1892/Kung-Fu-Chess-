#pragma once

#include <memory>
#include <optional>
#include "common/Position.hpp"
#include "pieces/Piece.hpp"

namespace kungfu {

class ICollisionSystem {
public:
    virtual ~ICollisionSystem() = default;

    // Returns the piece at 'to' if one exists, regardless of color.
    virtual std::optional<PiecePtr> findCollision(const Position& from, const Position& to) const = 0;

    // Returns true when 'to' is occupied by an enemy piece (capture is allowed).
    virtual bool isCapture(const Position& from, const Position& to) const = 0;

    // Returns true when 'to' is occupied by a friendly piece (move must be blocked).
    virtual bool isFriendlyBlock(const Position& from, const Position& to) const = 0;

    // Returns true when all squares between 'from' and 'to' (exclusive) are empty.
    virtual bool isPathClear(const Position& from, const Position& to) const = 0;
};

using CollisionSystemPtr = std::shared_ptr<ICollisionSystem>;

}  // namespace kungfu
