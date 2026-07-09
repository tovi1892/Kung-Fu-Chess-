#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

# נתיבי בסיס של הפרויקט
ROOT = Path(__file__).resolve().parents[1]
OUTPUT_FILE = ROOT / "generated_single_file.cpp"

# סדר פתרון התלויות עבור קובצי ה-CPP (מבטיח הגדרות לפני שימושים)
CPP_ORDER = [
    "src/common/Position.cpp",
    "src/pieces/Piece.cpp",
    "src/pieces/King.cpp",
    "src/pieces/Queen.cpp",
    "src/pieces/Rook.cpp",
    "src/pieces/Bishop.cpp",
    "src/pieces/Knight.cpp",
    "src/pieces/Pawn.cpp",
    "src/board/Board.cpp",
    "src/movement/MovementSystem.cpp",
    "src/collision/CollisionSystem.cpp",
    "src/rules/RuleEngine.cpp",
    "src/game/Game.cpp"
]

# קוד ה-Driver האינטראקטיבי שיוזרק בסוף הקובץ המאוחד
DRIVER_CODE = """
// ============================================================================
// PLATFORM HARNESS DRIVER (AUTOMATICALLY APPENDED BY BUILD_SINGLE_CPP.PY)
// ============================================================================

namespace {

bool is_valid_piece_token(const std::string& token) {
    static const std::vector<std::string> valid = {
        ".", "wK", "wQ", "wR", "wB", "wN", "wP",
        "bK", "bQ", "bR", "bB", "bN", "bP"
    };
    return std::find(valid.begin(), valid.end(), token) != valid.end();
}

std::vector<std::string> split_ws(const std::string& text) {
    std::istringstream ss(text);
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

kungfu::PiecePtr createPieceFromToken(const std::string& token, const kungfu::Position& pos) {
    if (token == "." || token.size() < 2) return nullptr;
    kungfu::PlayerColor color = (token[0] == 'w') ? kungfu::PlayerColor::White : kungfu::PlayerColor::Black;
    char type = token[1];
    switch (type) {
        case 'K': return std::make_shared<kungfu::King>(color, pos);
        case 'Q': return std::make_shared<kungfu::Queen>(color, pos);
        case 'R': return std::make_shared<kungfu::Rook>(color, pos);
        case 'B': return std::make_shared<kungfu::Bishop>(color, pos);
        case 'N': return std::make_shared<kungfu::Knight>(color, pos);
        case 'P': return std::make_shared<kungfu::Pawn>(color, pos);
        default: return nullptr;
    }
}

struct ActiveJump {
    kungfu::Position cell;
    int landTimeMs;
};

} // namespace

int main() {
    std::vector<std::string> all_lines;
    std::string line;
    bool has_sections = false;

    // קריאת כל קלט השורות
    while (std::getline(std::cin, line)) {
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), trimmed.end());

        if (trimmed == "Board:" || trimmed == "Commands:") {
            has_sections = true;
        }
        all_lines.push_back(trimmed);
    }

    if (!has_sections) {
        // --- תרחיש א': שלב א' בלבד (לוח בלבד ללא פקודות) ---
        std::vector<std::vector<std::string>> parsed_board;
        bool valid = true;
        size_t col_count = 0;

        for (const auto& row_line : all_lines) {
            std::vector<std::string> tokens = split_ws(row_line);
            if (tokens.empty()) {
                continue;
            }

            if (parsed_board.empty()) {
                col_count = tokens.size();
                if (col_count == 0) {
                    valid = false;
                }
            } else {
                if (tokens.size() != col_count) {
                    valid = false;
                }
            }

            for (const auto& token : tokens) {
                if (!is_valid_piece_token(token)) {
                    valid = false;
                }
            }
            parsed_board.push_back(tokens);
        }

        if (parsed_board.empty() || !valid) {
            std::cout << "Invalid board" << std::endl;
            return 1;
        }

        size_t rows = parsed_board.size();
        size_t cols = col_count;
        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < cols; ++c) {
                std::cout << parsed_board[r][c] << (c + 1 < cols ? " " : "");
            }
            std::cout << "\\n";
        }
        return 0;
    }

    // --- תרחיש ב': משחק מלא עם כותרות ופקודות ---
    std::vector<std::vector<std::string>> board_grid;
    std::vector<std::string> command_lines;
    bool reading_board = false;
    bool reading_commands = false;

    for (const auto& trimmed : all_lines) {
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed == "Board:") {
            reading_board = true;
            reading_commands = false;
            continue;
        }
        if (trimmed == "Commands:") {
            reading_board = false;
            reading_commands = true;
            continue;
        }
        if (reading_board) {
            board_grid.push_back(split_ws(trimmed));
        } else if (reading_commands) {
            command_lines.push_back(trimmed);
        }
    }

    // יצירת לוח מודולרי מהמחלקה האמיתית שלך בממדים שהוסקו דינמית
    int rows = board_grid.size();
    int cols = (rows > 0) ? board_grid[0].size() : 0;
    auto board = std::make_shared<kungfu::Board>(rows, cols);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto piece = createPieceFromToken(board_grid[r][c], kungfu::Position(r, c));
            if (piece) {
                board->placePiece(piece, kungfu::Position(r, c));
            }
        }
    }

    // הרצת המשחק
    kungfu::Game game(board);
    game.start();

    int currentTimeMs = 0;
    std::vector<ActiveJump> activeJumps;

    for (const std::string& command : command_lines) {
        std::istringstream ss(command);
        std::string verb;
        ss >> verb;
        if (verb == "click") {
            int x = 0;
            int y = 0;
            ss >> x >> y;
            game.click(x, y);
        } else if (verb == "jump") {
            int x = 0;
            int y = 0;
            ss >> x >> y;
            kungfu::Position pos{y / 100, x / 100};
            if (game.tryJump(pos)) {
                activeJumps.push_back(ActiveJump{pos, currentTimeMs + 1000});
            }
        } else if (verb == "wait") {
            int ms = 0;
            ss >> ms;
            currentTimeMs += ms;
            
            // הרצת שעון המשחק ופתרון תנועות רגילות
            game.wait(ms);
            
            // פתרון קפיצות שהגיע זמן נחיתתן (1,000 מילישניות מתחילת הקפיצה)
            for (auto it = activeJumps.begin(); it != activeJumps.end(); ) {
                if (currentTimeMs >= it->landTimeMs) {
                    game.resolveJump(it->cell);
                    it = activeJumps.erase(it); // הסרה בטוחה מתוך לולאה
                } else {
                    ++it;
                }
            }
        } else if (verb == "print") {
            std::string target;
            ss >> target;
            if (target == "board") {
                game.printBoard(std::cout);
            }
        }
    }

    return 0;
}
"""


class Amalgamator:
    def __init__(self, root_dir: Path):
        self.root_dir = root_dir
        self.visited_headers = set()
        # אתחול מראש של ספריות המערכת הנדרשות עבור ה-Driver המרכזי
        self.system_includes = {"sstream", "string", "cctype", "vector", "iostream", "memory", "algorithm"}

    def resolve_file(self, file_path: Path) -> str:
        if not file_path.exists():
            return ""

        content = file_path.read_text(encoding="utf-8")
        lines = content.splitlines()
        result = []

        for line in lines:
            # דילוג על #pragma once
            if line.strip().startswith("#pragma once"):
                continue

            # ריכוז ספריות מערכת: #include <vector>
            system_match = re.match(r'^\s*#include\s*<([^>]+)>', line)
            if system_match:
                self.system_includes.add(system_match.group(1))
                continue

            # החלפת #include מקומי בתוכן הקובץ המקורי (פעם אחת בלבד)
            local_match = re.match(r'^\s*#include\s*"([^"]+)"', line)
            if local_match:
                inc_rel_path = local_match.group(1)
                # חיפוש תחת תיקיית include
                inc_path = self.root_dir / "include" / inc_rel_path
                if not inc_path.exists():
                    # חיפוש יחסי למיקום הקובץ הנוכחי
                    inc_path = (file_path.parent / inc_rel_path).resolve()

                if inc_path.exists():
                    resolved_abs = inc_path.resolve()
                    if resolved_abs not in self.visited_headers:
                        self.visited_headers.add(resolved_abs)
                        # קריאה רקורסיבית לפענוח ה-Header
                        header_content = self.resolve_file(resolved_abs)
                        result.append(header_content)
                continue

            result.append(line)

        return "\n".join(result)


def build_output(output_path: Path) -> None:
    amalgamator = Amalgamator(ROOT)
    cpp_bodies = []

    # עיבוד כל קובצי ה-CPP לפי הסדר שהוגדר
    for cpp_rel in CPP_ORDER:
        cpp_path = ROOT / cpp_rel
        if cpp_path.exists():
            resolved_cpp = amalgamator.resolve_file(cpp_path)
            cpp_bodies.append(f"// === Source: {cpp_rel} ===")
            cpp_bodies.append(resolved_cpp)
            cpp_bodies.append("\n")

    # בניית בלוק ההכללות של ספריות מערכת
    sorted_systems = sorted(list(amalgamator.system_includes))
    system_block = "\n".join(f"#include <{sys}>" for sys in sorted_systems)

    # הרכבת הקובץ המאוחד הסופי
    final_output = []
    final_output.append("// ============================================================================")
    final_output.append("// AUTOMATICALLY GENERATED SINGLE-FILE REPRESENTATION BY BUILD_SINGLE_CPP.PY")
    final_output.append("// ============================================================================\n")
    final_output.append(system_block)
    final_output.append("\n")
    final_output.extend(cpp_bodies)
    final_output.append(DRIVER_CODE)

    # כתיבה לקובץ היעד
    output_path.write_text("\n".join(final_output), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Build a single consolidated C++ file from the modular source.")
    parser.add_argument("--output", default=str(OUTPUT_FILE), help="Path to the generated .cpp file")
    args = parser.parse_args()

    output_path = Path(args.output).resolve()
    build_output(output_path)
    print(f"[Success] Single unified file generated at: {output_path}")


if __name__ == "__main__":
    main()