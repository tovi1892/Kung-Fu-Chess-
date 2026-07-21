#pragma once

namespace kungfu {

// Visual/geometric tuning constants specific to the OpenCV-backed UI - pixel offsets,
// font scale, line thickness, sizes. Colors stay local `static const cv::Scalar`s next
// to the function that draws with them (see BoardRenderer.cpp/ScoreboardRenderer.cpp) -
// this file is only for the numbers, not the colors.
struct RenderConfig {
    // --- Top strip (room-id label area, reserved above the board and both side panels) ---
    // Real added height, fed into both OpenCvView's own height_ and mapper_'s offsetY -
    // not just a text position - so the board, its labels, and both panels all shift
    // down to make room for it. See Scoreboard::roomLabel / OpenCvView's constructor.
    static constexpr int kTopStripHeightPx = 32;
    static constexpr int kRoomLabelYPx = 22;
    static constexpr double kRoomLabelFontSize = 0.5;
    static constexpr int kRoomLabelTextThickness = 1;

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
    // Name/score/header Y-anchors are pushed down by kTopStripHeightPx so their content
    // starts below the room-id label strip instead of colliding with it.
    static constexpr double kPanelNameFontSize = 0.65;
    static constexpr int kPanelNameTextThickness = 2;
    static constexpr int kPanelNameYPx = 34 + kTopStripHeightPx;

    static constexpr double kPanelScoreFontSize = 0.45;
    static constexpr int kPanelScoreTextThickness = 1;
    static constexpr int kPanelScoreYPx = 60 + kTopStripHeightPx;

    static constexpr double kPanelHeaderFontSize = 0.42;
    static constexpr int kPanelHeaderTextThickness = 1;
    static constexpr int kPanelHeaderYPx = 95 + kTopStripHeightPx;

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

    // --- Username prompt native window layout ---
    static constexpr int kUsernamePromptWindowWidth = 320;
    static constexpr int kUsernamePromptWindowHeight = 160;
    static constexpr int kUsernamePromptLabelX = 20;
    static constexpr int kUsernamePromptLabelY = 20;
    static constexpr int kUsernamePromptLabelWidth = 260;
    static constexpr int kUsernamePromptLabelHeight = 20;
    static constexpr int kUsernamePromptEditX = 20;
    static constexpr int kUsernamePromptEditY = 45;
    static constexpr int kUsernamePromptEditWidth = 260;
    static constexpr int kUsernamePromptEditHeight = 24;
    // Play and Room sit side by side on the same row.
    static constexpr int kUsernamePromptButtonY = 90;
    static constexpr int kUsernamePromptButtonWidth = 90;
    static constexpr int kUsernamePromptButtonHeight = 30;
    static constexpr int kUsernamePromptPlayButtonX = 70;
    static constexpr int kUsernamePromptRoomButtonX = 180;

    // --- Room popup window layout (opened by the username prompt's Room button) ---
    static constexpr int kRoomPromptWindowWidth = 320;
    static constexpr int kRoomPromptWindowHeight = 160;
    static constexpr int kRoomPromptLabelX = 20;
    static constexpr int kRoomPromptLabelY = 20;
    static constexpr int kRoomPromptLabelWidth = 260;
    static constexpr int kRoomPromptLabelHeight = 20;
    static constexpr int kRoomPromptEditX = 20;
    static constexpr int kRoomPromptEditY = 45;
    static constexpr int kRoomPromptEditWidth = 260;
    static constexpr int kRoomPromptEditHeight = 24;
    // Create, Join, and Cancel sit side by side on the same row.
    static constexpr int kRoomPromptButtonY = 90;
    static constexpr int kRoomPromptButtonWidth = 80;
    static constexpr int kRoomPromptButtonHeight = 30;
    static constexpr int kRoomPromptCreateButtonX = 30;
    static constexpr int kRoomPromptJoinButtonX = 120;
    static constexpr int kRoomPromptCancelButtonX = 210;

    // --- OpenCvView window ---
    static constexpr int kDefaultWindowBoardSizePx = 800;
    static constexpr int kDefaultSidePanelWidthPx = 260;
    static constexpr int kDividerThicknessPx = 1;
    // How long the render loop's cv::waitKey blocks per frame - the same concept as
    // Server/main.cpp's kTickMs (coincidentally the same value), but this is client-
    // side frame pacing for polling input/repainting, not the server's simulation tick.
    static constexpr int kFrameWaitMs = 16;
};

}  // namespace kungfu
