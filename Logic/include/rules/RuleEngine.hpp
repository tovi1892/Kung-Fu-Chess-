#pragma once

#include <memory>
#include "model/IBoard.hpp"
#include "model/Position.hpp"
#include "rules/IRuleEngine.hpp"

namespace kungfu {

// The only IRuleEngine implementation. See IRuleEngine for what each method
// does - this class doesn't change those contracts, just implements them.
class RuleEngine : public IRuleEngine {
public:
    explicit RuleEngine(BoardPtr board);

    MoveValidation validateMove(const Position& from, const Position& to) const override;
    bool isValidMove(const Position& from, const Position& to) const override;
    bool isPawnPromotion(const Position& to, PlayerColor color) const override;

private:
    BoardPtr board_;
};

using RuleEnginePtr = std::shared_ptr<RuleEngine>;

}  // namespace kungfu
