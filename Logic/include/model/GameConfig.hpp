#pragma once

namespace kungfu {

// Constants every layer of the game shares: board dimensions and every
// real-time timing value, all in one place so they can't drift out of sync
// with each other.
struct GameConfig {
    static constexpr int kBoardSize = 8;
    static constexpr int kDefaultMoveStep = 1;

    // Fixed pixel size of one board cell, and the speed a piece travels at
    // (1x game speed). kMsPerCell is the time it takes to cross one cell -
    // the base unit RealTimeArbiter builds all of its move timing from.
    static constexpr int kCellSizePx = 100;
    static constexpr int kPieceSpeedPxPerSec = 100;
    static constexpr int kMsPerCell = 1000 * kCellSizePx / kPieceSpeedPxPerSec;

    // How long a piece stays on cooldown after arriving from a move, before
    // it's selectable again. Scales with RealTimeArbiter's speedMultiplier,
    // same as kMsPerCell.
    static constexpr int kBaseCooldownMs = 1500;

    // How long a piece stays airborne after jumping (experimental "jump"
    // mechanic) before landing back to Idle on its own. Scales with
    // speedMultiplier too. Chosen to comfortably outlast a knight's fixed
    // 2-cell (2 * kMsPerCell) approach, so a jump can dodge a knight attack
    // and not just an adjacent sliding piece.
    static constexpr int kBaseAirborneMs = 2000;

    // Pawn start rows (the row a pawn sits on at game start).
    static constexpr int kWhitePawnStartRow = 1;
    static constexpr int kBlackPawnStartRow = 6;

    // Promotion rows (the row a pawn reaches to become a queen).
    static constexpr int kWhitePawnPromotionRow = 7;
    static constexpr int kBlackPawnPromotionRow = 0;
};

}  // namespace kungfu
