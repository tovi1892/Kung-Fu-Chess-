#include "Protocol.hpp"

#include <sstream>

#include "io/BoardParser.hpp"

namespace kungfu::net {

namespace {

// Single-char color/type codes and 2-char "wR"-style piece tokens, matching the exact
// convention BoardParser/BoardPrinter already use for the text board DSL.
char colorChar(PlayerColor color) { return color == PlayerColor::White ? 'w' : 'b'; }
PlayerColor colorFromChar(char c) { return c == 'w' ? PlayerColor::White : PlayerColor::Black; }

char typeChar(PieceType type) {
    switch (type) {
        case PieceType::King:   return 'K';
        case PieceType::Queen:  return 'Q';
        case PieceType::Rook:   return 'R';
        case PieceType::Bishop: return 'B';
        case PieceType::Knight: return 'N';
        case PieceType::Pawn:   return 'P';
    }
    return 'P';
}

PieceType typeFromChar(char c) {
    switch (c) {
        case 'K': return PieceType::King;
        case 'Q': return PieceType::Queen;
        case 'R': return PieceType::Rook;
        case 'B': return PieceType::Bishop;
        case 'N': return PieceType::Knight;
        default:  return PieceType::Pawn;
    }
}

std::string pieceToken(PlayerColor color, PieceType type) {
    return std::string(1, colorChar(color)) + std::string(1, typeChar(type));
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        const std::string trimmed = BoardParser::trim(line);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }
    return lines;
}

}  // namespace

std::string encodeClick(int row, int col) {
    return "CLICK " + std::to_string(row) + " " + std::to_string(col);
}

std::string encodeWelcome(PlayerColor color) {
    return std::string("WELCOME ") + colorChar(color);
}

std::string encodeState(const std::vector<RenderPiece>& pieces) {
    std::ostringstream out;
    out << "STATE\n" << std::fixed;
    out.precision(6);
    for (const auto& rp : pieces) {
        out << rp.id << " " << pieceToken(static_cast<PlayerColor>(rp.color), static_cast<PieceType>(rp.type))
            << " " << rp.x << " " << rp.y << " " << rp.state << " " << rp.cooldownMs << " "
            << rp.cooldownTotalMs << "\n";
    }
    out << "END";
    return out.str();
}

std::string encodeMoveStarted(const MoveStarted& event) {
    std::ostringstream out;
    out << "MOVE " << pieceToken(event.color, event.type) << " " << event.from.row() << " "
        << event.from.col() << " " << event.to.row() << " " << event.to.col() << " "
        << (event.isCapture ? 1 : 0) << " " << event.elapsedMs << " " << event.notation;
    return out.str();
}

std::string encodePieceCaptured(const PieceCaptured& event) {
    std::ostringstream out;
    out << "CAPTURE " << colorChar(event.capturingColor) << " " << typeChar(event.capturedType) << " "
        << event.at.row() << " " << event.at.col();
    return out.str();
}

std::string encodeGameStarted() {
    return "GAME_START";
}

std::string encodeGameEnded(const GameEnded& event) {
    return std::string("GAME_END ") + colorChar(event.winner);
}

DecodedMessage decode(const std::string& text) {
    const auto lines = splitLines(text);
    if (lines.empty()) {
        return std::monostate{};
    }

    const auto tokens = BoardParser::split(lines[0]);
    if (tokens.empty()) {
        return std::monostate{};
    }
    const std::string& cmd = tokens[0];

    if (cmd == "CLICK" && tokens.size() >= 3) {
        return ClickMessage{std::stoi(tokens[1]), std::stoi(tokens[2])};
    }

    if (cmd == "WELCOME" && tokens.size() >= 2 && !tokens[1].empty()) {
        return WelcomeMessage{colorFromChar(tokens[1][0])};
    }

    if (cmd == "STATE") {
        StateMessage msg;
        for (size_t i = 1; i < lines.size(); ++i) {
            if (lines[i] == "END") {
                break;
            }
            const auto fields = BoardParser::split(lines[i]);
            if (fields.size() < 7 || fields[1].size() < 2) {
                continue;  // malformed line - skip rather than abort the whole snapshot
            }
            RenderPiece rp;
            rp.id = static_cast<uintptr_t>(std::stoull(fields[0]));
            rp.color = static_cast<int>(colorFromChar(fields[1][0]));
            rp.type = static_cast<int>(typeFromChar(fields[1][1]));
            rp.x = std::stod(fields[2]);
            rp.y = std::stod(fields[3]);
            rp.state = std::stoi(fields[4]);
            rp.cooldownMs = std::stod(fields[5]);
            rp.cooldownTotalMs = std::stod(fields[6]);
            msg.pieces.push_back(rp);
        }
        return msg;
    }

    if (cmd == "MOVE" && tokens.size() >= 9 && tokens[1].size() >= 2) {
        MoveStarted event;
        event.color = colorFromChar(tokens[1][0]);
        event.type = typeFromChar(tokens[1][1]);
        event.from = Position(std::stoi(tokens[2]), std::stoi(tokens[3]));
        event.to = Position(std::stoi(tokens[4]), std::stoi(tokens[5]));
        event.isCapture = tokens[6] == "1";
        event.elapsedMs = std::stoi(tokens[7]);
        event.notation = tokens[8];
        return event;
    }

    if (cmd == "CAPTURE" && tokens.size() >= 5 && !tokens[1].empty() && !tokens[2].empty()) {
        PieceCaptured event;
        event.capturingColor = colorFromChar(tokens[1][0]);
        event.capturedType = typeFromChar(tokens[2][0]);
        event.at = Position(std::stoi(tokens[3]), std::stoi(tokens[4]));
        return event;
    }

    if (cmd == "GAME_START") {
        return GameStarted{};
    }

    if (cmd == "GAME_END" && tokens.size() >= 2 && !tokens[1].empty()) {
        return GameEnded{colorFromChar(tokens[1][0])};
    }

    return std::monostate{};
}

}  // namespace kungfu::net
