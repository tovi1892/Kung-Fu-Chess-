#include <memory>
#include "board/Board.hpp"
#include "BoardParser.hpp"

int main() {
    auto board = std::make_shared<kungfu::Board>();
    kungfu::BoardParser parser;
    parser.parseInput(std::cin, board);
    return 0;
}
