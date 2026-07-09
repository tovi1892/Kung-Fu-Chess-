// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include "pieces/King.hpp"

int main() {
    kungfu::King piece(kungfu::PlayerColor::White, kungfu::Position(0, 0));

    assert(piece.type() == kungfu::PieceType::King);
    assert(piece.color() == kungfu::PlayerColor::White);
    assert(piece.position() == kungfu::Position(0, 0));
    assert(piece.isMovable());

    piece.setPosition(kungfu::Position(1, 1));
    assert(piece.position() == kungfu::Position(1, 1));

    return 0;
}
