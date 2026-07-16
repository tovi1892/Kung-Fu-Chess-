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

// True when 'occupant' should block a square from being a legal target: it's
// a friendly piece that is actually staying put. A friendly piece that is
// itself PieceState::Moving is already on its way out - by the time any new
// move actually reaches that square, real-time resolution in RealTimeArbiter
// will find it vacated (or, if the timing is close, resolve the encounter
// dynamically there) - so its square is legal to request the instant it
// starts moving, not only once it finishes crossing into the next cell.
bool blocksAsStationaryFriendly(const std::optional<Piece*>& occupant, PlayerColor movingColor) {
    return occupant.has_value() && occupant.value() != nullptr &&
           occupant.value()->color() == movingColor &&
           occupant.value()->state() != PieceState::Moving;
}

// Walks outward from 'piece' one direction at a time, all the way to the edge
// of the board. Squares occupied by a stationary friendly piece are never
// legal targets (never legal to capture your own piece), but - unlike
// standard chess - anything beyond a blocker (friendly or enemy) is still a
// legal *request*: this team's real-time rules allow requesting a move
// "through" pieces, since whatever is in the way might move out of it before
// this piece actually gets there. Whether the move actually reaches that far,
// or stops/captures short, is resolved dynamically during the motion by
// RealTimeArbiter - not here.
void collectSlidingDestinations(const IBoard& board,
                                 const Piece& piece,
                                 const std::vector<std::pair<int, int>>& directions,
                                 std::set<Position>& out) {
    const Position& from = piece.position();
    for (const auto& [dr, dc] : directions) {
        Position candidate(from.row() + dr, from.col() + dc);
        while (isInBounds(board, candidate)) {
            const auto occupant = board.pieceAt(candidate);
            if (!blocksAsStationaryFriendly(occupant, piece.color())) {
                out.insert(candidate);
            }
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
        if (!blocksAsStationaryFriendly(occupant, piece.color())) {
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

std::set<Position> PawnRules::legalDestinations(const IBoard& board, const Piece& piece) const {
    std::set<Position> out;
    const Position& from = piece.position();
    const bool isWhite = piece.color() == PlayerColor::White;
    const int forward = isWhite ? 1 : -1;
    const int startRow = isWhite ? GameConfig::kWhitePawnStartRow : GameConfig::kBlackPawnStartRow;

    // A pawn's forward step never captures, so any occupant (friendly or
    // enemy) blocks it - except one that's already Moving and thus vacating,
    // same exception as the sliding/stepped pieces above.
    auto blocksForwardStep = [](const std::optional<Piece*>& occupant) {
        return occupant.has_value() && occupant.value() != nullptr &&
               occupant.value()->state() != PieceState::Moving;
    };

    const Position oneStep(from.row() + forward, from.col());
    if (isInBounds(board, oneStep)) {
        const auto occupant = board.pieceAt(oneStep);
        if (!blocksForwardStep(occupant)) {
            out.insert(oneStep);

            if (from.row() == startRow) {
                const Position twoStep(from.row() + 2 * forward, from.col());
                if (isInBounds(board, twoStep)) {
                    const auto occupant2 = board.pieceAt(twoStep);
                    if (!blocksForwardStep(occupant2)) {
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
