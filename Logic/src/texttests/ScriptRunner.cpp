#include "texttests/ScriptRunner.hpp"

#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include "engine/GameEngine.hpp"
#include "input/Controller.hpp"
#include "io/BoardParser.hpp"
#include "io/BoardPrinter.hpp"

namespace kungfu {

namespace {

std::string toUpper(std::string s) {
    for (auto& ch : s) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return s;
}

}  // namespace

bool ScriptRunner::run(std::istream& in, std::ostream& out) const {
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }

    size_t i = 0;
    while (i < lines.size() && BoardParser::trim(lines[i]) != "Board:") {
        ++i;
    }
    if (i == lines.size()) {
        return false;
    }
    ++i;

    std::vector<std::string> boardLines;
    while (i < lines.size()) {
        std::string t = BoardParser::trim(lines[i]);
        if (t.empty()) { ++i; continue; }
        if (t == "Commands:" || t == "Commands") { ++i; break; }
        boardLines.push_back(t);
        ++i;
    }

    std::shared_ptr<Board> board;
    std::string errorMessage;
    if (!BoardParser().parseBoard(boardLines, board, errorMessage)) {
        out << errorMessage << "\n";
        return false;
    }

    auto game = std::make_shared<GameEngine>(board, nullptr);
    game->start();
    Controller controller(game);
    BoardPrinter printer;

    for (; i < lines.size(); ++i) {
        const std::string trimmed = BoardParser::trim(lines[i]);
        if (trimmed.empty()) {
            continue;
        }

        const auto parts = BoardParser::split(trimmed);
        const std::string cmd = toUpper(parts[0]);

        if (cmd == "CLICK" && parts.size() >= 3) {
            controller.handleClick(std::stoi(parts[1]), std::stoi(parts[2]));
            continue;
        }

        if (cmd == "WAIT" && parts.size() >= 2) {
            game->wait(std::stoi(parts[1]));
            continue;
        }

        if (cmd == "PRINT" && parts.size() >= 2 && toUpper(parts[1]) == "BOARD") {
            printer.print(out, *game->getBoard(), game->boardRows(), game->boardCols());
            continue;
        }
    }

    return true;
}

}  // namespace kungfu
