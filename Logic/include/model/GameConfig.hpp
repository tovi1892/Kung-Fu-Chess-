#pragma once

namespace kungfu {

struct GameConfig {
    static constexpr int kBoardSize = 8;
    static constexpr int kDefaultMoveStep = 1;

    // Fixed real-time movement constants (course spec section 10).
    static constexpr int kCellSizePx = 100;
    static constexpr int kPieceSpeedPxPerSec = 100;
    static constexpr int kMsPerCell = 1000 * kCellSizePx / kPieceSpeedPxPerSec;

    // Pawn start rows (the row a pawn sits on at game start).
    static constexpr int kWhitePawnStartRow = 1;
    static constexpr int kBlackPawnStartRow = 6;

    // Promotion rows (the row a pawn reaches to become a queen).
    static constexpr int kWhitePawnPromotionRow = 7;
    static constexpr int kBlackPawnPromotionRow = 0;
};

}  // namespace kungfu
