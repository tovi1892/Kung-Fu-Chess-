#pragma once

namespace kungfu {

struct GameConfig {
    static constexpr int kBoardSize = 8;
    static constexpr int kDefaultMoveStep = 1;

    // Fixed real-time movement constants (course spec section 10).
    static constexpr int kCellSizePx = 100;
    static constexpr int kPieceSpeedPxPerSec = 100;
    static constexpr int kMsPerCell = 1000 * kCellSizePx / kPieceSpeedPxPerSec;

    // Base cooldown after a piece finishes a move, before it is selectable
    // again. Scales with RealTimeArbiter's speedMultiplier, same as kMsPerCell.
    static constexpr int kBaseCooldownMs = 1000;

    // How long a piece stays airborne after jumping (experimental "jump"
    // mechanic) before landing back to Idle on its own. Scales with
    // RealTimeArbiter's speedMultiplier, same as kBaseCooldownMs. Chosen to
    // comfortably cover a knight's fixed 2-cell (2 * kMsPerCell) approach, so
    // a jump can dodge a knight attack and not just adjacent sliding pieces.
    static constexpr int kBaseAirborneMs = 2000;

    // Pawn start rows (the row a pawn sits on at game start).
    static constexpr int kWhitePawnStartRow = 1;
    static constexpr int kBlackPawnStartRow = 6;

    // Promotion rows (the row a pawn reaches to become a queen).
    static constexpr int kWhitePawnPromotionRow = 7;
    static constexpr int kBlackPawnPromotionRow = 0;
};

}  // namespace kungfu
