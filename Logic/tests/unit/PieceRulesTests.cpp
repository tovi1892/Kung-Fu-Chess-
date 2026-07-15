#include <catch2/catch.hpp>

#include "model/Board.hpp"
#include "model/pieces/Bishop.hpp"
#include "model/pieces/King.hpp"
#include "model/pieces/Knight.hpp"
#include "model/pieces/Pawn.hpp"
#include "model/pieces/Queen.hpp"
#include "model/pieces/Rook.hpp"
#include "rules/PieceRules.hpp"

using namespace kungfu;

TEST_CASE("RookRules: slides across empty row and column", "[piece_rules][rook]") {
    Board board;
    auto* rook = new Rook(PlayerColor::White, Position(3, 3));
    board.placePiece(std::unique_ptr<Piece>(rook), Position(3, 3));

    auto dest = RookRules{}.legalDestinations(board, *rook);
    REQUIRE(dest.count(Position(0, 3)) == 1);
    REQUIRE(dest.count(Position(7, 3)) == 1);
    REQUIRE(dest.count(Position(3, 0)) == 1);
    REQUIRE(dest.count(Position(3, 7)) == 1);
    REQUIRE(dest.count(Position(4, 4)) == 0);  // not straight
}

TEST_CASE("RookRules: stops before a friendly blocker", "[piece_rules][rook]") {
    Board board;
    auto* rook = new Rook(PlayerColor::White, Position(3, 3));
    board.placePiece(std::unique_ptr<Piece>(rook), Position(3, 3));
    board.placePiece(std::make_unique<Rook>(PlayerColor::White, Position(3, 5)), Position(3, 5));

    auto dest = RookRules{}.legalDestinations(board, *rook);
    REQUIRE(dest.count(Position(3, 4)) == 1);
    REQUIRE(dest.count(Position(3, 5)) == 0);  // friendly: excluded
    REQUIRE(dest.count(Position(3, 6)) == 0);  // beyond the blocker
}

TEST_CASE("RookRules: captures an enemy blocker but does not pass it", "[piece_rules][rook]") {
    Board board;
    auto* rook = new Rook(PlayerColor::White, Position(3, 3));
    board.placePiece(std::unique_ptr<Piece>(rook), Position(3, 3));
    board.placePiece(std::make_unique<Rook>(PlayerColor::Black, Position(3, 5)), Position(3, 5));

    auto dest = RookRules{}.legalDestinations(board, *rook);
    REQUIRE(dest.count(Position(3, 4)) == 1);
    REQUIRE(dest.count(Position(3, 5)) == 1);  // enemy: capturable
    REQUIRE(dest.count(Position(3, 6)) == 0);  // beyond the captured piece
}

TEST_CASE("BishopRules: moves diagonally and not straight", "[piece_rules][bishop]") {
    Board board;
    auto* bishop = new Bishop(PlayerColor::White, Position(3, 3));
    board.placePiece(std::unique_ptr<Piece>(bishop), Position(3, 3));

    auto dest = BishopRules{}.legalDestinations(board, *bishop);
    REQUIRE(dest.count(Position(0, 0)) == 1);
    REQUIRE(dest.count(Position(6, 6)) == 1);
    REQUIRE(dest.count(Position(0, 6)) == 1);
    REQUIRE(dest.count(Position(3, 0)) == 0);  // straight, not diagonal
}

TEST_CASE("QueenRules: combines rook and bishop movement", "[piece_rules][queen]") {
    Board board;
    auto* queen = new Queen(PlayerColor::White, Position(3, 3));
    board.placePiece(std::unique_ptr<Piece>(queen), Position(3, 3));

    auto dest = QueenRules{}.legalDestinations(board, *queen);
    REQUIRE(dest.count(Position(3, 0)) == 1);  // straight
    REQUIRE(dest.count(Position(0, 0)) == 1);  // diagonal
    REQUIRE(dest.count(Position(4, 6)) == 0);  // neither straight nor diagonal
}

TEST_CASE("KnightRules: jumps over blockers", "[piece_rules][knight]") {
    Board board;
    auto* knight = new Knight(PlayerColor::White, Position(3, 3));
    board.placePiece(std::unique_ptr<Piece>(knight), Position(3, 3));
    // Surround the knight on all four adjacent squares; none of these block an L-jump.
    board.placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(2, 3)), Position(2, 3));
    board.placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(4, 3)), Position(4, 3));
    board.placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(3, 2)), Position(3, 2));
    board.placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(3, 4)), Position(3, 4));

    auto dest = KnightRules{}.legalDestinations(board, *knight);
    REQUIRE(dest.count(Position(1, 2)) == 1);
    REQUIRE(dest.count(Position(5, 4)) == 1);
    REQUIRE(dest.count(Position(2, 3)) == 0);  // not an L-shape from (3,3)
}

TEST_CASE("KnightRules: destination square still respects friendly/enemy occupancy", "[piece_rules][knight]") {
    Board board;
    auto* knight = new Knight(PlayerColor::White, Position(3, 3));
    board.placePiece(std::unique_ptr<Piece>(knight), Position(3, 3));
    board.placePiece(std::make_unique<Pawn>(PlayerColor::White, Position(1, 2)), Position(1, 2));
    board.placePiece(std::make_unique<Pawn>(PlayerColor::Black, Position(1, 4)), Position(1, 4));

    auto dest = KnightRules{}.legalDestinations(board, *knight);
    REQUIRE(dest.count(Position(1, 2)) == 0);  // friendly landing square: excluded
    REQUIRE(dest.count(Position(1, 4)) == 1);  // enemy landing square: capture allowed
}

TEST_CASE("KingRules: moves one cell only", "[piece_rules][king]") {
    Board board;
    auto* king = new King(PlayerColor::White, Position(3, 3));
    board.placePiece(std::unique_ptr<Piece>(king), Position(3, 3));

    auto dest = KingRules{}.legalDestinations(board, *king);
    REQUIRE(dest.size() == 8);
    REQUIRE(dest.count(Position(2, 2)) == 1);
    REQUIRE(dest.count(Position(4, 4)) == 1);
    REQUIRE(dest.count(Position(5, 3)) == 0);  // two cells away
}

TEST_CASE("PawnRules: moves and captures according to the simplified pawn rules", "[piece_rules][pawn]") {
    Board board;
    auto* pawn = new Pawn(PlayerColor::White, Position(2, 3));
    board.placePiece(std::unique_ptr<Piece>(pawn), Position(2, 3));
    board.placePiece(std::make_unique<Rook>(PlayerColor::Black, Position(3, 4)), Position(3, 4));
    board.placePiece(std::make_unique<Rook>(PlayerColor::White, Position(3, 2)), Position(3, 2));

    auto dest = PawnRules{}.legalDestinations(board, *pawn);
    REQUIRE(dest.count(Position(3, 3)) == 1);   // forward, empty
    REQUIRE(dest.count(Position(3, 4)) == 1);   // diagonal enemy: capture
    REQUIRE(dest.count(Position(3, 2)) == 0);   // diagonal friendly: no capture
}

TEST_CASE("PawnRules: forward move is blocked by any occupant, friendly or enemy", "[piece_rules][pawn]") {
    Board board;
    auto* pawn = new Pawn(PlayerColor::White, Position(2, 3));
    board.placePiece(std::unique_ptr<Piece>(pawn), Position(2, 3));
    board.placePiece(std::make_unique<Rook>(PlayerColor::Black, Position(3, 3)), Position(3, 3));

    auto dest = PawnRules{}.legalDestinations(board, *pawn);
    REQUIRE(dest.count(Position(3, 3)) == 0);  // straight ahead is never a capture
}

TEST_CASE("PawnRules: two-step is only available from the start row with a clear path", "[piece_rules][pawn]") {
    Board board;
    auto* pawn = new Pawn(PlayerColor::White, Position(1, 3));
    board.placePiece(std::unique_ptr<Piece>(pawn), Position(1, 3));

    auto dest = PawnRules{}.legalDestinations(board, *pawn);
    REQUIRE(dest.count(Position(2, 3)) == 1);
    REQUIRE(dest.count(Position(3, 3)) == 1);

    board.placePiece(std::make_unique<Rook>(PlayerColor::White, Position(2, 3)), Position(2, 3));
    auto destBlocked = PawnRules{}.legalDestinations(board, *pawn);
    REQUIRE(destBlocked.count(Position(3, 3)) == 0);  // middle square occupied: no two-step
}
