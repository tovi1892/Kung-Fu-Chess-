#pragma once

namespace kungfu {

// Visual/geometric tuning constants specific to the OpenCV-backed UI - pixel offsets,
// font scale, line thickness, sizes. Colors stay local `static const cv::Scalar`s next
// to the function that draws with them (see BoardRenderer.cpp/ScoreboardRenderer.cpp) -
// this file is only for the numbers, not the colors.
struct RenderConfig {
    // --- Board coordinate labels (a-h / 1-8) ---
    static constexpr double kLabelFontSize = 0.5;
    static constexpr int kLabelTextThickness = 1;
    // Rough half-character nudge so a label reads as centered under/beside its cell,
    // rather than starting exactly at the cell's edge.
    static constexpr int kLabelHorizontalCenteringOffsetPx = -5;
    static constexpr int kFileLabelTopOffsetPx = -12;
    static constexpr int kFileLabelBottomOffsetPx = 24;
    static constexpr int kRankLabelVerticalCenteringOffsetPx = 5;

    // --- Selection / last-move cell outline ---
    static constexpr int kHighlightOutlineThickness = 3;

    // --- Rest ring (post-move cooldown / post-jump short rest progress meter) ---
    static constexpr int kRestRingInsetPx = 4;
    static constexpr int kRestRingTrackThickness = 3;
    static constexpr int kRestRingProgressThickness = 4;
    // cv::ellipse measures angles clockwise from 3 o'clock, so -90 is 12 o'clock - see
    // drawRestRing's own comment in BoardRenderer.hpp for why the ring starts there.
    static constexpr double kRestRingStartAngleDeg = -90.0;

    // --- Game-lifecycle banner ("GAME START", "WHITE WINS", ...) ---
    static constexpr double kBannerFontSize = 1.0;
    static constexpr int kBannerTextThickness = 2;
    static constexpr int kBannerPaddingXPx = 24;
    static constexpr int kBannerPaddingYPx = 18;
    static constexpr int kBannerTextLineHeightPx = 34;
    static constexpr int kBannerTextBaselineOffsetPx = 10;

    // --- Scoreboard side panel ---
    static constexpr double kPanelNameFontSize = 0.65;
    static constexpr int kPanelNameTextThickness = 2;
    static constexpr int kPanelNameYPx = 34;

    static constexpr double kPanelScoreFontSize = 0.45;
    static constexpr int kPanelScoreTextThickness = 1;
    static constexpr int kPanelScoreYPx = 60;

    static constexpr double kPanelHeaderFontSize = 0.42;
    static constexpr int kPanelHeaderTextThickness = 1;
    static constexpr int kPanelHeaderYPx = 95;

    static constexpr int kPanelTimeColumnOffsetPx = 18;
    static constexpr int kPanelMoveColumnOffsetPx = 10;

    static constexpr int kPanelDividerXInsetPx = 10;
    static constexpr int kPanelDividerYOffsetPx = 8;
    static constexpr int kPanelDividerWidthInsetPx = 20;
    static constexpr int kPanelDividerThicknessPx = 1;

    static constexpr int kPanelRowStartYOffsetPx = 28;
    static constexpr int kPanelRowHeightPx = 20;
    static constexpr int kPanelBottomPaddingPx = 12;

    static constexpr double kPanelMoveRowFontSize = 0.4;
    static constexpr int kPanelMoveRowTextThickness = 1;
};

}  // namespace kungfu
