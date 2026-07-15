#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>
#include "model/Board.hpp"
#include "engine/GameEngine.hpp"
#include "game/UIInputAdapter.hpp"

namespace kungfu {

class BoardParser {
public:
    bool parseInput(std::istream& in, std::shared_ptr<Board>& board) const;

private:
    static std::string trim(const std::string& s);
    static std::vector<std::string> split(const std::string& s);
    static bool createPieceFromToken(const std::string& token, const Position& pos, std::unique_ptr<Piece>& outPiece);
    bool parseBoardLines(const std::vector<std::string>& boardLines,
                         std::shared_ptr<Board>& board,
                         std::string& errorMessage) const;
    void executeCommands(const std::shared_ptr<GameEngine>& game,
                         const std::shared_ptr<UIInputAdapter>& adapter,
                         const std::vector<std::string>& commands) const;
};

}  // namespace kungfu
