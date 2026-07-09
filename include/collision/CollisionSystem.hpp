#pragma once

#include <memory>
#include <optional>
#include "board/IBoard.hpp"
#include "collision/ICollisionSystem.hpp"
#include "common/Position.hpp"
#include "pieces/Piece.hpp"

namespace kungfu {

class CollisionSystem : public ICollisionSystem {
public:
    explicit CollisionSystem(BoardPtr board);

    std::optional<PiecePtr> findCollision(const Position& from, const Position& to) const override;
    bool isCapture(const Position& from, const Position& to) const override;
    bool isFriendlyBlock(const Position& from, const Position& to) const override;
    bool isPathClear(const Position& from, const Position& to) const override;

private:
    BoardPtr board_;
};

}  // namespace kungfu
