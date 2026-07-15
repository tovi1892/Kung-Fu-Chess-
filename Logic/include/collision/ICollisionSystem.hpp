#pragma once

#include <memory>
#include <optional>
#include "model/IBoard.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

// Shared geometric helper: true when every square strictly between 'from' and
// 'to' is empty. Used both by CollisionSystem and by the sliding-piece
// movement rules (RookRules/BishopRules/QueenRules) so path-blocking has a
// single implementation.
bool isPathClear(const IBoard& board, const Position& from, const Position& to);

class ICollisionSystem {
public:
    virtual ~ICollisionSystem() = default;

    // Returns the piece at 'to' if one exists, regardless of color.
    virtual std::optional<Piece*> findCollision(const Position& from, const Position& to) const = 0;

    // Returns true when 'to' is occupied by an enemy piece (capture is allowed).
    virtual bool isCapture(const Position& from, const Position& to) const = 0;

    // Returns true when 'to' is occupied by a friendly piece (move must be blocked).
    virtual bool isFriendlyBlock(const Position& from, const Position& to) const = 0;

    // Returns true when all squares between 'from' and 'to' (exclusive) are empty.
    virtual bool isPathClear(const Position& from, const Position& to) const = 0;
};

using CollisionSystemPtr = std::shared_ptr<ICollisionSystem>;

}  // namespace kungfu
