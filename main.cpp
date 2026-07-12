#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cctype>

#include "Board.hpp"
#include "Game.hpp"
#include "UIInputAdapter.hpp"
#include "King.hpp"
#include "Queen.hpp"
#include "Rook.hpp"
#include "Bishop.hpp"
#include "Knight.hpp"
#include "Pawn.hpp"
#include "GameConfig.hpp"


using namespace kungfu;

static inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static inline std::vector<std::string> split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

int main() {
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(std::cin, line)) {
        lines.push_back(line);
    }

    size_t i = 0;
    while (i < lines.size() && trim(lines[i]) != "Board:") ++i;
    if (i == lines.size()) return 0; 
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
        auto board = std::make_shared<Board>();
        auto game = std::make_shared<Game>(board, nullptr, nullptr);
        game->start();
    } else {
        auto firstTokens = split(boardLines[0]);
        int cols = (int)firstTokens.size();
        auto board = std::make_shared<Board>((int)boardLines.size(), cols);
        
        for (size_t r = 0; r < boardLines.size(); ++r) {
            auto tokens = split(boardLines[r]);
            if ((int)tokens.size() != cols) {
                std::cout << "ERROR ROW_WIDTH_MISMATCH" << std::endl;
                return 0;
            }
            for (int c = 0; c < cols; ++c) {
                if (tokens[c] == "." || tokens[c] == "_") continue;
                if (tokens[c].size() < 2) { std::cout << "ERROR UNKNOWN_TOKEN" << std::endl; return 0; }
                
                char color = (char)std::tolower(tokens[c][0]);
                char kind = (char)std::toupper(tokens[c][1]);
                PlayerColor pc = (color == 'w') ? PlayerColor::White : PlayerColor::Black;
                
                PiecePtr piece;
                Position pos((int)r, c);
                if (kind == 'K') piece = std::make_shared<King>(pc, pos);
                else if (kind == 'Q') piece = std::make_shared<Queen>(pc, pos);
                else if (kind == 'R') piece = std::make_shared<Rook>(pc, pos);
                else if (kind == 'B') piece = std::make_shared<Bishop>(pc, pos);
                else if (kind == 'N') piece = std::make_shared<Knight>(pc, pos);
                else if (kind == 'P') piece = std::make_shared<Pawn>(pc, pos);
                else { std::cout << "ERROR UNKNOWN_TOKEN" << std::endl; return 0; }
                
                if (!board->placePiece(piece, pos)) { std::cout << "ERROR UNKNOWN_TOKEN" << std::endl; return 0; }
            }
        }
        
        auto gameHolder = std::make_shared<std::shared_ptr<Game>>();
        auto adapter = std::make_shared<UIInputAdapter>([gameHolder]() -> IGameInputTarget& { return **gameHolder; });
        auto game = std::make_shared<Game>(board, nullptr, adapter);
        *gameHolder = game;
        game->start();

        for (; i < lines.size(); ++i) {
            auto cmdLine = trim(lines[i]);
            if (cmdLine.empty()) continue;
            auto parts = split(cmdLine);
            std::string cmd = parts[0];
            for (auto &c : cmd) c = (char)std::toupper(c);

            if (cmd == "PRINT" || cmd == "PRINTBOARD") game->printBoard(std::cout);
            else if (cmd == "CLICK") { if (parts.size() >= 3) game->click(std::stoi(parts[1]), std::stoi(parts[2])); }
            else if (cmd == "WAIT") { if (parts.size() >= 2) game->wait(std::stoi(parts[1])); }
        }
    }
    return 0;
}