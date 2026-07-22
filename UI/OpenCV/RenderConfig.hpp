#pragma once

namespace kungfu {

// Visual/geometric tuning constants specific to the OpenCV-backed UI - pixel offsets,
// font scale, line thickness, sizes. Colors stay local `static constexpr Color`s next
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

    // --- Login screen native window layout (the first of the three pre-game screens) ---
    static constexpr int kLoginScreenWindowWidth = 340;
    static constexpr int kLoginScreenWindowHeight = 260;
    static constexpr int kLoginScreenTitleX = 20;
    static constexpr int kLoginScreenTitleY = 16;
    static constexpr int kLoginScreenTitleWidth = 280;
    static constexpr int kLoginScreenTitleHeight = 28;
    static constexpr int kLoginScreenLabelX = 20;
    static constexpr int kLoginScreenLabelWidth = 280;
    static constexpr int kLoginScreenLabelHeight = 18;
    static constexpr int kLoginScreenUsernameLabelY = 60;
    static constexpr int kLoginScreenUsernameEditY = 80;
    // Password label/edit reuse the username field's X/width/height - same shape, just a
    // row lower. The edit control itself additionally gets ES_PASSWORD (masked input).
    static constexpr int kLoginScreenPasswordLabelY = 112;
    static constexpr int kLoginScreenPasswordEditY = 132;
    static constexpr int kLoginScreenEditX = 20;
    static constexpr int kLoginScreenEditWidth = 280;
    static constexpr int kLoginScreenEditHeight = 24;
    // Login-failure error text, shown only when non-empty.
    static constexpr int kLoginScreenErrorLabelY = 164;
    static constexpr int kLoginScreenErrorLabelHeight = 20;
    static constexpr int kLoginScreenButtonX = 110;
    static constexpr int kLoginScreenButtonY = 194;
    static constexpr int kLoginScreenButtonWidth = 120;
    static constexpr int kLoginScreenButtonHeight = 34;

    // --- Login result window layout (shown once LOGIN_OK arrives) ---
    static constexpr int kLoginResultWindowWidth = 340;
    static constexpr int kLoginResultWindowHeight = 170;
    static constexpr int kLoginResultMessageX = 20;
    static constexpr int kLoginResultMessageY = 30;
    static constexpr int kLoginResultMessageWidth = 300;
    static constexpr int kLoginResultMessageHeight = 60;
    static constexpr int kLoginResultButtonX = 110;
    static constexpr int kLoginResultButtonY = 110;
    static constexpr int kLoginResultButtonWidth = 120;
    static constexpr int kLoginResultButtonHeight = 34;

    // --- Room choice screen native window layout (the second of the three pre-game screens) ---
    static constexpr int kRoomChoiceScreenWindowWidth = 340;
    static constexpr int kRoomChoiceScreenWindowHeight = 320;
    static constexpr int kRoomChoiceScreenTitleX = 20;
    static constexpr int kRoomChoiceScreenTitleY = 16;
    static constexpr int kRoomChoiceScreenTitleWidth = 300;
    static constexpr int kRoomChoiceScreenTitleHeight = 26;
    static constexpr int kRoomChoiceScreenWelcomeX = 20;
    static constexpr int kRoomChoiceScreenWelcomeY = 48;
    static constexpr int kRoomChoiceScreenWelcomeWidth = 300;
    static constexpr int kRoomChoiceScreenWelcomeHeight = 20;
    static constexpr int kRoomChoiceScreenRadioX = 20;
    static constexpr int kRoomChoiceScreenRadioWidth = 280;
    static constexpr int kRoomChoiceScreenRadioHeight = 22;
    static constexpr int kRoomChoiceScreenQuickMatchRadioY = 84;
    static constexpr int kRoomChoiceScreenCreateRoomRadioY = 112;
    static constexpr int kRoomChoiceScreenJoinRoomRadioY = 140;
    static constexpr int kRoomChoiceScreenRoomLabelX = 40;
    static constexpr int kRoomChoiceScreenRoomLabelY = 170;
    static constexpr int kRoomChoiceScreenRoomLabelWidth = 100;
    static constexpr int kRoomChoiceScreenRoomLabelHeight = 18;
    static constexpr int kRoomChoiceScreenRoomEditX = 40;
    static constexpr int kRoomChoiceScreenRoomEditY = 190;
    static constexpr int kRoomChoiceScreenRoomEditWidth = 260;
    static constexpr int kRoomChoiceScreenRoomEditHeight = 24;
    // Log Out and Next sit side by side on the same row.
    static constexpr int kRoomChoiceScreenButtonY = 240;
    static constexpr int kRoomChoiceScreenButtonHeight = 34;
    static constexpr int kRoomChoiceScreenLogOutButtonX = 20;
    static constexpr int kRoomChoiceScreenLogOutButtonWidth = 100;
    static constexpr int kRoomChoiceScreenNextButtonX = 190;
    static constexpr int kRoomChoiceScreenNextButtonWidth = 110;

    // --- Play confirm screen native window layout (the third of the three pre-game screens) ---
    static constexpr int kPlayConfirmScreenWindowWidth = 340;
    static constexpr int kPlayConfirmScreenWindowHeight = 230;
    static constexpr int kPlayConfirmScreenTitleX = 20;
    static constexpr int kPlayConfirmScreenTitleY = 16;
    static constexpr int kPlayConfirmScreenTitleWidth = 300;
    static constexpr int kPlayConfirmScreenTitleHeight = 26;
    static constexpr int kPlayConfirmScreenModeX = 20;
    static constexpr int kPlayConfirmScreenModeY = 56;
    static constexpr int kPlayConfirmScreenModeWidth = 300;
    static constexpr int kPlayConfirmScreenModeHeight = 44;
    static constexpr int kPlayConfirmScreenErrorLabelY = 108;
    static constexpr int kPlayConfirmScreenErrorLabelHeight = 20;
    // Back and Play sit side by side on the same row.
    static constexpr int kPlayConfirmScreenButtonY = 150;
    static constexpr int kPlayConfirmScreenButtonHeight = 34;
    static constexpr int kPlayConfirmScreenBackButtonX = 20;
    static constexpr int kPlayConfirmScreenBackButtonWidth = 100;
    static constexpr int kPlayConfirmScreenPlayButtonX = 190;
    static constexpr int kPlayConfirmScreenPlayButtonWidth = 130;

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
