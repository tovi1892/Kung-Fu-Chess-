#include "rules/PieceRules.hpp"

#include <utility>
#include <vector>

#include "model/GameConfig.hpp"

namespace kungfu {

namespace {

bool isInBounds(const IBoard& board, const Position& pos) {
    return pos.row() >= 0 && pos.row() < board.rows() &&
           pos.col() >= 0 && pos.col() < board.cols();
}

// Walks outward from 'piece' one direction at a time, adding every empty
// square and stopping at the first occupied square (adding it only if it's
// an enemy piece, i.e. a capture). This is the single implementation of
// "sliding pieces do not pass through blocking pieces" (section 9).
void collectSlidingDestinations(const IBoard& board,
                                 const Piece& piece,
                                 const std::vector<std::pair<int, int>>& directions,
                                 std::set<Position>& out) {
    const Position& from = piece.position();
    for (const auto& [dr, dc] : directions) {
        Position candidate(from.row() + dr, from.col() + dc);
        while (isInBounds(board, candidate)) {
            const auto occupant = board.pieceAt(candidate);
            if (occupant.has_value() && occupant.value() != nullptr) {
                if (occupant.value()->color() != piece.color()) {
                    out.insert(candidate);
                }
                break;
            }
            out.insert(candidate);
            candidate = Position(candidate.row() + dr, candidate.col() + dc);
        }
    }
}

void collectSteppedDestinations(const IBoard& board,
                                 const Piece& piece,
                                 const std::vector<std::pair<int, int>>& offsets,
                                 std::set<Position>& out) {
    const Position& from = piece.position();
    for (const auto& [dr, dc] : offsets) {
        Position candidate(from.row() + dr, from.col() + dc);
        if (!isInBounds(board, candidate)) {
            continue;
        }
        const auto occupant = board.pieceAt(candidate);
        if (!occupant.has_value() || occupant.value() == nullptr ||
            occupant.value()->color() != piece.color()) {
            out.insert(candidate);
        }
    }
}

}  // namespace

std::set<Position> RookRules::legalDestinations(const IBoard& board, const Piece& piece) const {
    std::set<Position> out;
    collectSlidingDestinations(board, piece, {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}, out);
    return out;
}

std::set<Position> BishopRules::legalDestinations(const IBoard& board, const Piece& piece) const {
    std::set<Position> out;
    collectSlidingDestinations(board, piece, {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}, out);
    return out;
}

std::set<Position> QueenRules::legalDestinations(const IBoard& board, const Piece& piece) const {
    std::set<Position> out;
    collectSlidingDestinations(
        board, piece,
        {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}, out);
    return out;
}

std::set<Position> KnightRules::legalDestinations(const IBoard& board, const Piece& piece) const {
    std::set<Position> out;
    collectSteppedDestinations(
        board, piece,
        {{2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}}, out);
    return out;
}

std::set<Position> KingRules::legalDestinations(const IBoard& board, const Piece& piece) const {
    std::set<Position> out;
    collectSteppedDestinations(
        board, piece,
        {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}}, out);
    return out;
}

// Pawn behavior: single step forward, diagonal capture, plus the team's own
// double-step-from-start extension (kept alongside the common-route rules,
// not part of the graded common route itself).
std::set<Position> PawnRules::legalDestinations(const IBoard& board, const Piece& piece) const {
    std::set<Position> out;
    const Position& from = piece.position();
    const bool isWhite = piece.color() == PlayerColor::White;
    const int forward = isWhite ? 1 : -1;
    const int startRow = isWhite ? GameConfig::kWhitePawnStartRow : GameConfig::kBlackPawnStartRow;

    const Position oneStep(from.row() + forward, from.col());
    if (isInBounds(board, oneStep)) {
        const auto occupant = board.pieceAt(oneStep);
        const bool oneStepEmpty = !occupant.has_value() || occupant.value() == nullptr;
        if (oneStepEmpty) {
            out.insert(oneStep);

            if (from.row() == startRow) {
                const Position twoStep(from.row() + 2 * forward, from.col());
                if (isInBounds(board, twoStep)) {
                    const auto occupant2 = board.pieceAt(twoStep);
                    if (!occupant2.has_value() || occupant2.value() == nullptr) {
                        out.insert(twoStep);
                    }
                }
            }
        }
    }

    for (int dc : {-1, 1}) {
        const Position capture(from.row() + forward, from.col() + dc);
        if (!isInBounds(board, capture)) {
            continue;
        }
        const auto occupant = board.pieceAt(capture);
        if (occupant.has_value() && occupant.value() != nullptr &&
            occupant.value()->color() != piece.color()) {
            out.insert(capture);
        }
    }

    return out;
}

const IPieceRules& pieceRulesFor(PieceType type) {
    static const RookRules rook;
    static const BishopRules bishop;
    static const QueenRules queen;
    static const KnightRules knight;
    static const KingRules king;
    static const PawnRules pawn;

    switch (type) {
        case PieceType::Rook: return rook;
        case PieceType::Bishop: return bishop;
        case PieceType::Queen: return queen;
        case PieceType::Knight: return knight;
        case PieceType::King: return king;
        case PieceType::Pawn: return pawn;
    }
    return pawn;
}

}  // namespace kungfu
