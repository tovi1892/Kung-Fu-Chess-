#pragma once

#include <memory>
#include <optional>
#include "model/IBoard.hpp"
#include "collision/ICollisionSystem.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

// The only ICollisionSystem implementation. See ICollisionSystem for what
// each method does - this class doesn't change those contracts, just
// implements them. Note: PieceRules and RealTimeArbiter each do their own
// inline occupancy checks rather than going through this class - it exists
// as the documented, independently-tested version of the same geometry.
class CollisionSystem : public ICollisionSystem {
public:
    explicit CollisionSystem(BoardPtr board);

    std::optional<Piece*> findCollision(const Position& from, const Position& to) const override;
    bool isCapture(const Position& from, const Position& to) const override;
    bool isFriendlyBlock(const Position& from, const Position& to) const override;
    bool isPathClear(const Position& from, const Position& to) const override;

private:
    BoardPtr board_;
};

}  // namespace kungfu