#include <catch2/catch.hpp>
#include <memory>
#include "model/Board.hpp"
#include "collision/CollisionSystem.hpp"
#include "model/pieces/King.hpp"

TEST_CASE("Collision detection", "[collision]") {
    auto board = std::make_shared<kungfu::Board>();
    kungfu::CollisionSystem collision(board);

    SECTION("Finds collision between pieces") {
        board->placePiece(std::make_unique<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0)), kungfu::Position(0, 0));
        board->placePiece(std::make_unique<kungfu::King>(kungfu::PlayerColor::Black, kungfu::Position(1, 1)), kungfu::Position(1, 1));

        auto collisionPiece = collision.findCollision(kungfu::Position(0, 0), kungfu::Position(1, 1));
        REQUIRE(collisionPiece.has_value());
        REQUIRE(collisionPiece.value()->type() == kungfu::PieceType::King);
    }
}