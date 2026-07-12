#pragma once

#include <memory>
#include "IBoard.hpp"
#include "Position.hpp"
#include "MovementSystem.hpp"
#include "IRuleEngine.hpp"

namespace kungfu {

class RuleEngine : public IRuleEngine {
public:
    explicit RuleEngine(BoardPtr board);

    MoveValidation validateMove(const Position& from, const Position& to) const override;
    bool isValidMove(const Position& from, const Position& to) const override;
    bool isPawnPromotion(const Position& to, PlayerColor color) const override;

private:
    BoardPtr board_;
    MovementSystem movementSystem_;
};

using RuleEnginePtr = std::shared_ptr<RuleEngine>;

}  // namespace kungfu
