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
    // (~700ms at 1x speed.)
    static constexpr int kCellSizePx = 80;
    static constexpr int kPieceSpeedPxPerSec = 150;
    static constexpr int kMsPerCell = 1000 * kCellSizePx / kPieceSpeedPxPerSec;

    // How long a piece stays on cooldown after arriving from a move, before
    // it's selectable again. Scales with RealTimeArbiter's speedMultiplier,
    // same as kMsPerCell.
    static constexpr int kBaseCooldownMs = 1500;

    // How long a piece stays airborne after jumping (experimental "jump"
    // mechanic) before landing into a short rest on its own (see
    // kBaseShortRestMs below). Scales with speedMultiplier too. A jump can
    // still dodge an approaching knight if timed well (jumping shortly
    // before the knight's fixed 2-cell hop lands), but no longer
    // automatically outlasts a knight's full approach the way it did at
    // higher values - it's meant to be a real gamble, not a guaranteed dodge.
    static constexpr int kBaseAirborneMs = 700;

    // How long a piece rests after landing from a jump (naturally, or via a
    // counter-kill) before it's selectable again - shorter than the regular
    // post-move cooldown above, since a jump's landing is a lighter action
    // than a full move. Scales with speedMultiplier, same as kBaseCooldownMs.
    static constexpr int kBaseShortRestMs = kBaseCooldownMs / 2;

    // Pawn start rows (the row a pawn sits on at game start).
    static constexpr int kWhitePawnStartRow = 1;
    static constexpr int kBlackPawnStartRow = 6;

    // Promotion rows (the row a pawn reaches to become a queen).
    static constexpr int kWhitePawnPromotionRow = 7;
    static constexpr int kBlackPawnPromotionRow = 0;
};

}  // namespace kungfu
