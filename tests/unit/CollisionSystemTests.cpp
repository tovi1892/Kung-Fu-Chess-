// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include <memory>
#include "board/Board.hpp"
#include "collision/CollisionSystem.hpp"
#include "pieces/King.hpp"

int main() {
    auto board = std::make_shared<kungfu::Board>();
    auto attacker = std::make_shared<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    auto defender = std::make_shared<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(1, 1));

    board->placePiece(attacker, kungfu::Position(0, 0));
    board->placePiece(defender, kungfu::Position(1, 1));

    kungfu::CollisionSystem collision(board);
    const auto collisionPiece = collision.findCollision(kungfu::Position(0, 0), kungfu::Position(1, 1));

    assert(collisionPiece.has_value());
    assert(collisionPiece.value() == defender);

    const auto noCollision = collision.findCollision(kungfu::Position(2, 2), kungfu::Position(3, 3));
    assert(!noCollision.has_value());

    {
        auto b = std::make_shared<kungfu::Board>();
        auto blocker = std::make_shared<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(0, 3));
        b->placePiece(blocker, kungfu::Position(0, 3));

        kungfu::CollisionSystem coll(b);
        assert(!coll.isPathClear(kungfu::Position(0, 0), kungfu::Position(0, 5))); // חסום על ידי הכלי ב-(0,3)
        assert(coll.isPathClear(kungfu::Position(0, 0), kungfu::Position(0, 2)));  // הנתיב עד (0,2) פנוי
    }

    return 0;
}