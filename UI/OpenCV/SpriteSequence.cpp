#include "SpriteSequence.hpp"

namespace kungfu {

const SpriteSequence& PieceAnimationSet::forState(PieceState state) const {
    switch (state) {
        case PieceState::Moving:    return move;
        case PieceState::Airborne:  return jump;
        case PieceState::Cooldown:  return longRest;
        case PieceState::ShortRest: return shortRest;
        case PieceState::Idle:
        case PieceState::Captured:
        default:
            return idle;
    }
}

}  // namespace kungfu
