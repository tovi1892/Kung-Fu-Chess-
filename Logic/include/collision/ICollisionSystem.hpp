#pragma once

#include <memory>
#include <optional>
#include "model/IBoard.hpp"
#include "model/Position.hpp"
#include "model/pieces/Piece.hpp"

namespace kungfu {

bool isPathClear(const IBoard& board, const Position& from, const Position& to);

class ICollisionSystem {
public:
    virtual ~ICollisionSystem() = default;

    virtual std::optional<Piece*> findCollision(const Position& from, const Position& to) const = 0;

    virtual bool isCapture(const Position& from, const Position& to) const = 0;

    virtual bool isFriendlyBlock(const Position& from, const Position& to) const = 0;

    virtual bool isPathClear(const Position& from, const Position& to) const = 0;
};

using CollisionSystemPtr = std::shared_ptr<ICollisionSystem>;

}  // namespace kungfu
