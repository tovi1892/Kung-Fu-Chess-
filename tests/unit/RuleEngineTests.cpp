// Repository: https://github.com/Naama00/kong-fu-chess.git

#include <cassert>
#include <memory>
#include "board/Board.hpp"
#include "pieces/King.hpp"
#include "rules/RuleEngine.hpp"

int main() {
    auto board = std::make_shared<kungfu::Board>();
    auto king = std::make_shared<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    board->placePiece(king, kungfu::Position(0, 0));

    kungfu::RuleEngine engine(board);

    const auto okValidation = engine.validateMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
    assert(okValidation.is_valid);
    assert(okValidation.reason == "ok");

    const auto outsideBoardValidation = engine.validateMove(kungfu::Position(9, 9), kungfu::Position(8, 8));
    assert(!outsideBoardValidation.is_valid);
    assert(outsideBoardValidation.reason == "outside_board");

    auto emptyBoard = std::make_shared<kungfu::Board>();
    kungfu::RuleEngine emptyEngine(emptyBoard);
    const auto emptySourceValidation = emptyEngine.validateMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
    assert(!emptySourceValidation.is_valid);
    assert(emptySourceValidation.reason == "empty_source");

    auto friendlyBoard = std::make_shared<kungfu::Board>();
    auto friendlyKing = std::make_shared<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    auto friendlyTarget = std::make_shared<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(1, 1));
    friendlyBoard->placePiece(friendlyKing, kungfu::Position(0, 0));
    friendlyBoard->placePiece(friendlyTarget, kungfu::Position(1, 1));
    kungfu::RuleEngine friendlyEngine(friendlyBoard);
    const auto friendlyValidation = friendlyEngine.validateMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
    assert(!friendlyValidation.is_valid);
    assert(friendlyValidation.reason == "friendly_destination");

    auto illegalBoard = std::make_shared<kungfu::Board>();
    auto illegalKing = std::make_shared<kungfu::King>(kungfu::PlayerColor::White, kungfu::Position(0, 0));
    illegalBoard->placePiece(illegalKing, kungfu::Position(0, 0));
    kungfu::RuleEngine illegalEngine(illegalBoard);
    const auto illegalValidation = illegalEngine.validateMove(kungfu::Position(0, 0), kungfu::Position(2, 2));
    assert(!illegalValidation.is_valid);
    assert(illegalValidation.reason == "illegal_piece_move");

    assert(engine.isValidMove(kungfu::Position(0, 0), kungfu::Position(1, 1)) == true);
    assert(engine.isValidMove(kungfu::Position(0, 0), kungfu::Position(0, 0)) == false);
    assert(engine.isValidMove(kungfu::Position(0, 0), kungfu::Position(2, 2)) == false);
    assert(engine.isValidMove(kungfu::Position(9, 9), kungfu::Position(8, 8)) == false);

    return 0;
}
