#include "BoardParser.hpp"

#include <cctype>
#include <functional>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "model/pieces/Bishop.hpp"
#include "model/pieces/King.hpp"
#include "model/pieces/Knight.hpp"
#include "model/pieces/Pawn.hpp"
#include "model/pieces/Queen.hpp"
#include "model/pieces/Rook.hpp"
#include "model/GameConfig.hpp"
#include "engine/GameEngine.hpp"
#include "game/GameController.hpp"
#include "game/UIInputAdapter.hpp"

namespace kungfu {

std::string BoardParser::trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> BoardParser::split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) {
        out.push_back(tok);
    }
    return out;
}

bool BoardParser::createPieceFromToken(const std::string& token, const Position& pos, std::unique_ptr<Piece>& outPiece) {
    if (token.size() < 2) {
        return false;
    }

    char color = static_cast<char>(std::tolower(token[0]));
    char kind = static_cast<char>(std::toupper(token[1]));
    PlayerColor pc;
    if (color == 'w') {
        pc = PlayerColor::White;
    } else if (color == 'b') {
        pc = PlayerColor::Black;
    } else {
        return false;
    }

    static const std::unordered_map<std::string, std::function<std::unique_ptr<Piece>(PlayerColor, Position)>> creators = {
        {"K", [](PlayerColor color, Position position) { return std::make_unique<King>(color, position); }},
        {"Q", [](PlayerColor color, Position position) { return std::make_unique<Queen>(color, position); }},
        {"R", [](PlayerColor color, Position position) { return std::make_unique<Rook>(color, position); }},
        {"B", [](PlayerColor color, Position position) { return std::make_unique<Bishop>(color, position); }},
        {"N", [](PlayerColor color, Position position) { return std::make_unique<Knight>(color, position); }},
        {"P", [](PlayerColor color, Position position) { return std::make_unique<Pawn>(color, position); }}
    };

    auto it = creators.find(std::string(1, kind));
    if (it == creators.end()) {
        return false;
    }

    outPiece = it->second(pc, pos);
    return true;
}

bool BoardParser::parseBoardLines(const std::vector<std::string>& boardLines,
                                  std::shared_ptr<Board>& board,
                                  std::string& errorMessage) const {
    if (boardLines.empty()) {
        board = std::make_shared<Board>();
        return true;
    }

    auto firstTokens = split(boardLines[0]);
    int cols = static_cast<int>(firstTokens.size());
    int rows = static_cast<int>(boardLines.size());
    board = std::make_shared<Board>(rows, cols);

    for (int r = 0; r < rows; ++r) {
        auto tokens = split(boardLines[r]);
        if (static_cast<int>(tokens.size()) != cols) {
            errorMessage = "ERROR ROW_WIDTH_MISMATCH";
            return false;
        }

        for (int c = 0; c < cols; ++c) {
            const auto& tok = tokens[c];
            if (tok == "." || tok == "_") {
                continue;
            }

            std::unique_ptr<Piece> piece;
            if (!createPieceFromToken(tok, Position(r, c), piece)) {
                errorMessage = "ERROR UNKNOWN_TOKEN";
                return false;
            }

            if (!board->placePiece(std::move(piece), Position(r, c))) {
                errorMessage = "ERROR UNKNOWN_TOKEN";
                return false;
            }
        }
    }

    return true;
}

void BoardParser::executeCommands(const std::shared_ptr<GameEngine>& game,
                                  const std::shared_ptr<UIInputAdapter>& adapter,
                                  const std::vector<std::string>& commands) const {
    for (const auto& rawLine : commands) {
        auto line = trim(rawLine);
        if (line.empty()) {
            continue;
        }

        auto parts = split(line);
        if (parts.empty()) {
            continue;
        }

        std::string cmd = parts[0];
        for (auto& ch : cmd) {
            ch = static_cast<char>(std::toupper(ch));
        }

        if (cmd == "PRINT" || cmd == "PRINT_BOARD" || cmd == "PRINTBOARD") {
            game->printBoard(std::cout);
            continue;
        }

        if (cmd == "CLICK" || cmd == "CLICK_PIXEL") {
            if (parts.size() >= 3) {
                int x = std::stoi(parts[1]);
                int y = std::stoi(parts[2]);
                adapter->handleClick(x, y);
            }
            continue;
        }

        if (cmd == "CLICK_CELL") {
            if (parts.size() >= 3) {
                int r = std::stoi(parts[1]);
                int c = std::stoi(parts[2]);
                int x = c * GameConfig::kCellSizePx + GameConfig::kCellSizePx / 2;
                int y = r * GameConfig::kCellSizePx + GameConfig::kCellSizePx / 2;
                adapter->handleClick(x, y);
            }
            continue;
        }

        if (cmd == "WAIT") {
            if (parts.size() >= 2) {
                int ms = std::stoi(parts[1]);
                game->wait(ms);
            }
            continue;
        }

        if (cmd == "START") {
            game->start();
            continue;
        }

        if (cmd == "STOP") {
            game->stop();
            continue;
        }
    }
}

bool BoardParser::parseInput(std::istream& in, std::shared_ptr<Board>& board) const {
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }

    size_t i = 0;
    while (i < lines.size() && trim(lines[i]) != "Board:") {
        ++i;
    }
    if (i == lines.size()) {
        return false;
    }
    ++i;

    std::vector<std::string> boardLines;
    while (i < lines.size()) {
        std::string t = trim(lines[i]);
        if (t.empty()) { ++i; continue; }
        if (t == "Commands:" || t == "Commands") { ++i; break; }
        boardLines.push_back(t);
        ++i;
    }

    if (boardLines.empty()) {
        board = std::make_shared<Board>();
    } else {
        std::string errorMessage;
        if (!parseBoardLines(boardLines, board, errorMessage)) {
            std::cout << errorMessage << std::endl;
            return false;
        }
    }

    auto game = std::make_shared<GameEngine>(board, nullptr);
    game->start();
    auto controller = std::make_shared<GameController>(game);
    auto adapter = std::make_shared<UIInputAdapter>(
        [controller]() -> IGameInputTarget& { return *controller; });

    std::vector<std::string> commands;
    while (i < lines.size()) {
        commands.push_back(lines[i]);
        ++i;
    }
    executeCommands(game, adapter, commands);
    return true;
}

}  // namespace kungfu
