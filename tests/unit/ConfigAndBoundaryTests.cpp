// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include "common/GameConfig.hpp"
#include "movement/MovementSystem.hpp"

int main() {
    kungfu::MovementSystem movement;

    assert(kungfu::GameConfig::kBoardSize == 8);
    assert(kungfu::GameConfig::kDefaultMoveStep == 1);

    assert(!movement.isInBounds(kungfu::Position(-1, 0)));
    assert(!movement.isInBounds(kungfu::Position(0, -1)));
    assert(!movement.isInBounds(kungfu::Position(8, 0)));
    assert(!movement.isInBounds(kungfu::Position(0, 8)));
    assert(movement.isInBounds(kungfu::Position(0, 0)));
    assert(movement.isInBounds(kungfu::Position(7, 7)));

    return 0;
}
