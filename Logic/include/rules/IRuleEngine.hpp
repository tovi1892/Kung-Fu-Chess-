#pragma once

#include <string>
#include "common/Enums.hpp"
#include "common/Position.hpp"

namespace kungfu {

struct MoveValidation {
    bool is_valid;
    std::string reason;
};

class IRuleEngine {
public:
    virtual ~IRuleEngine() = default;
    virtual MoveValidation validateMove(const Position& from, const Position& to) const = 0;
    virtual bool isValidMove(const Position& from, const Position& to) const = 0;

    // Returns true if a pawn at 'to' with the given color should be promoted.
    virtual bool isPawnPromotion(const Position& to, PlayerColor color) const = 0;
};

}  // namespace kungfu
