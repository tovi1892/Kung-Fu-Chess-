#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>
#include "model/Board.hpp"

namespace kungfu {

// Text I/O: parses a textual board layout into a Board. Depends only on
// model data - knows nothing about Controller, RuleEngine, RealTimeArbiter,
// or GameEngine.
class BoardParser {
public:
    // Convenience entry point for callers that just need the initial board
    // (e.g. main.cpp): reads a "Board:" header followed by row lines, up to
    // EOF or an optional "Commands:" line (which is left unread, along with
    // anything after it).
    bool parseInput(std::istream& in, std::shared_ptr<Board>& board) const;

    // Parses already-split board row lines into a Board. Used by ScriptRunner,
    // which does its own Board:/Commands: line splitting.
    bool parseBoard(const std::vector<std::string>& boardLines,
                     std::shared_ptr<Board>& board,
                     std::string& errorMessage) const;

    // Strips leading/trailing whitespace.
    static std::string trim(const std::string& s);

    // Whitespace-tokenizes a line.
    static std::vector<std::string> split(const std::string& s);

private:
    static bool createPieceFromToken(const std::string& token, const Position& pos, std::unique_ptr<Piece>& outPiece);
};

}  // namespace kungfu
