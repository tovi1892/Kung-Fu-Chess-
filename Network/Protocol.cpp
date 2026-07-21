#include "Protocol.hpp"

#include <sstream>

#include "io/BoardParser.hpp"

namespace kungfu::net {

namespace {

// Decimal digits of precision for a piece's fractional x/y position in a STATE
// broadcast - enough for smooth interpolation without needlessly verbose lines.
constexpr int kCoordinatePrecisionDigits = 6;

// Single-char color/type codes and 2-char "wR"-style piece tokens, matching the exact
// convention BoardParser/BoardPrinter already use for the text board DSL.
char colorChar(PlayerColor color) { return color == PlayerColor::White ? 'w' : 'b'; }
PlayerColor colorFromChar(char c) { return c == 'w' ? PlayerColor::White : PlayerColor::Black; }

char typeChar(PieceType type) {
    return pieceTypeChar(type);
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

namespace {

char joinModeChar(JoinMode mode) {
    switch (mode) {
        case JoinMode::CreateRoom: return 'C';
        case JoinMode::JoinRoom:   return 'R';
        default:                  return 'Q';
    }
}

}  // namespace

std::string encodeLogin(const std::string& username, const std::string& password) {
    return "LOGIN " + username + " " + password;
}

std::string encodeLoginOk(int rating) {
    return "LOGIN_OK " + std::to_string(rating);
}

std::string encodeLoginFail(const std::string& reason) {
    return "LOGIN_FAIL " + reason;
}

std::string encodeJoin(JoinMode mode, const std::string& room) {
    std::string out = std::string("JOIN ") + joinModeChar(mode);
    if (mode == JoinMode::JoinRoom) {
        out += " " + room;
    }
    return out;
}

std::string encodeClick(int row, int col) {
    return "CLICK " + std::to_string(row) + " " + std::to_string(col);
}

std::string encodeWelcome(PlayerColor color) {
    return std::string("WELCOME ") + colorChar(color);
}

std::string encodeSpectate() {
    return "SPECTATE";
}

std::string encodeRoom(const std::string& key) {
    return "ROOM " + key;
}

std::string encodePlayers(const std::string& white, const std::string& black) {
    return "PLAYERS " + white + " " + black;
}

std::string encodeNoOpponent() {
    return "NO_OPPONENT";
}

std::string encodeForfeitWarning(PlayerColor disconnectedColor, int graceMs) {
    return std::string("FORFEIT_WARNING ") + colorChar(disconnectedColor) + " " + std::to_string(graceMs);
}

std::string encodeForfeit(PlayerColor winner) {
    return std::string("FORFEIT ") + colorChar(winner);
}

std::string encodeRatings(int whiteRating, int blackRating) {
    return "RATINGS " + std::to_string(whiteRating) + " " + std::to_string(blackRating);
}

std::string encodeState(const std::vector<RenderPiece>& pieces) {
    std::ostringstream out;
    out << "STATE\n" << std::fixed;
    out.precision(kCoordinatePrecisionDigits);
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

    if (cmd == "LOGIN" && tokens.size() >= 3) {
        return LoginMessage{tokens[1], tokens[2]};
    }

    if (cmd == "LOGIN_OK" && tokens.size() >= 2) {
        return LoginOkMessage{std::stoi(tokens[1])};
    }

    if (cmd == "LOGIN_FAIL" && tokens.size() >= 2) {
        return LoginFailMessage{tokens[1]};
    }

    if (cmd == "JOIN" && tokens.size() >= 2 && !tokens[1].empty()) {
        const char modeChar = tokens[1][0];
        if (modeChar == 'R') {
            if (tokens.size() < 3) {
                return std::monostate{};
            }
            return JoinMessage{JoinMode::JoinRoom, tokens[2]};
        }
        const JoinMode mode = modeChar == 'C' ? JoinMode::CreateRoom : JoinMode::QuickMatch;
        return JoinMessage{mode, ""};
    }

    if (cmd == "CLICK" && tokens.size() >= 3) {
        return ClickMessage{std::stoi(tokens[1]), std::stoi(tokens[2])};
    }

    if (cmd == "WELCOME" && tokens.size() >= 2 && !tokens[1].empty()) {
        return WelcomeMessage{colorFromChar(tokens[1][0])};
    }

    if (cmd == "SPECTATE") {
        return SpectateMessage{};
    }

    if (cmd == "ROOM" && tokens.size() >= 2) {
        return RoomMessage{tokens[1]};
    }

    if (cmd == "PLAYERS" && tokens.size() >= 3) {
        return PlayersMessage{tokens[1], tokens[2]};
    }

    if (cmd == "NO_OPPONENT") {
        return NoOpponentMessage{};
    }

    if (cmd == "FORFEIT_WARNING" && tokens.size() >= 3 && !tokens[1].empty()) {
        return ForfeitWarningMessage{colorFromChar(tokens[1][0]), std::stoi(tokens[2])};
    }

    if (cmd == "FORFEIT" && tokens.size() >= 2 && !tokens[1].empty()) {
        return ForfeitMessage{colorFromChar(tokens[1][0])};
    }

    if (cmd == "RATINGS" && tokens.size() >= 3) {
        return RatingsMessage{std::stoi(tokens[1]), std::stoi(tokens[2])};
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
