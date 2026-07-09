// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include "common/Position.hpp"

int main() {
    kungfu::Position p1(2, 3);
    kungfu::Position p2(2, 3);
    kungfu::Position p3(4, 5);

    assert(p1 == p2);
    assert(p1 != p3);
    assert(p1.row() == 2);
    assert(p1.col() == 3);

    return 0;
}
