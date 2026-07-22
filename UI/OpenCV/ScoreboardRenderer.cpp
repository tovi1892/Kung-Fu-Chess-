#include "ScoreboardRenderer.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "Img/Color.hpp"
#include "RenderConfig.hpp"

namespace kungfu {

namespace {

constexpr int kMsPerSecond = 1000;
constexpr int kSecondsPerMinute = 60;
constexpr int kMsPerMinute = kMsPerSecond * kSecondsPerMinute;

// mm:ss.mmm, matching the reference layout's TIME column.
std::string formatElapsed(double elapsedMs) {
    const int totalMs = static_cast<int>(elapsedMs);
    const int minutes = totalMs / kMsPerMinute;
    const int seconds = (totalMs / kMsPerSecond) % kSecondsPerMinute;
    const int millis = totalMs % kMsPerSecond;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ':'
        << std::setw(2) << seconds << '.'
        << std::setw(3) << millis;
    return oss.str();
}

}  // namespace

void drawPlayerPanel(Img& frame, const PlayerPanel& panel, int panelX, int panelWidth, int panelHeight) {
    static constexpr Color kNameColor{230, 200, 60};
    static constexpr Color kScoreColor{190, 190, 190};
    static constexpr Color kHeaderColor{225, 225, 225};
    static constexpr Color kMoveColor{205, 205, 205};
    static constexpr Color kDividerColor{90, 90, 90};

    auto centeredX = [&](const std::string& text, double fontSize, int thickness) {
        const int textWidth = frame.text_width(text, fontSize, thickness);
        return panelX + std::max(0, (panelWidth - textWidth) / 2);
    };

    // Rating is folded into the same name line ("alice (1215)") rather than its own row -
    // 0 means "not yet known" (pre-login, or the opponent's rating before a match this
    // client is part of has concluded), so it's simply omitted until then.
    std::string name = panel.name.empty() ? std::string("Player") : panel.name;
    if (panel.rating > 0) {
        name += " (" + std::to_string(panel.rating) + ")";
    }
    frame.put_text(name, centeredX(name, RenderConfig::kPanelNameFontSize, RenderConfig::kPanelNameTextThickness),
                    RenderConfig::kPanelNameYPx, RenderConfig::kPanelNameFontSize, kNameColor,
                    RenderConfig::kPanelNameTextThickness);

    const std::string scoreText = "Score: " + std::to_string(panel.score);
    frame.put_text(scoreText,
                    centeredX(scoreText, RenderConfig::kPanelScoreFontSize, RenderConfig::kPanelScoreTextThickness),
                    RenderConfig::kPanelScoreYPx, RenderConfig::kPanelScoreFontSize, kScoreColor,
                    RenderConfig::kPanelScoreTextThickness);

    const int headerY = RenderConfig::kPanelHeaderYPx;
    const int timeColX = panelX + RenderConfig::kPanelTimeColumnOffsetPx;
    const int moveColX = panelX + panelWidth / 2 + RenderConfig::kPanelMoveColumnOffsetPx;
    frame.put_text("TIME", timeColX, headerY, RenderConfig::kPanelHeaderFontSize, kHeaderColor,
                    RenderConfig::kPanelHeaderTextThickness);
    frame.put_text("MOVE", moveColX, headerY, RenderConfig::kPanelHeaderFontSize, kHeaderColor,
                    RenderConfig::kPanelHeaderTextThickness);
    frame.draw_rect(panelX + RenderConfig::kPanelDividerXInsetPx, headerY + RenderConfig::kPanelDividerYOffsetPx,
                     panelWidth - RenderConfig::kPanelDividerWidthInsetPx, RenderConfig::kPanelDividerThicknessPx,
                     kDividerColor);

    const int rowStartY = headerY + RenderConfig::kPanelRowStartYOffsetPx;
    const int rowHeight = RenderConfig::kPanelRowHeightPx;
    const int maxRows = std::max(0, (panelHeight - rowStartY - RenderConfig::kPanelBottomPaddingPx) / rowHeight);

    const int total = static_cast<int>(panel.moves.size());
    const int startIndex = std::max(0, total - maxRows);
    int y = rowStartY;
    for (int i = startIndex; i < total; ++i) {
        const auto& entry = panel.moves[i];
        frame.put_text(formatElapsed(entry.elapsedMs), timeColX, y, RenderConfig::kPanelMoveRowFontSize, kMoveColor,
                        RenderConfig::kPanelMoveRowTextThickness);
        frame.put_text(entry.notation, moveColX, y, RenderConfig::kPanelMoveRowFontSize, kMoveColor,
                        RenderConfig::kPanelMoveRowTextThickness);
        y += rowHeight;
    }
}

void drawRoomLabel(Img& frame, const std::string& roomLabel, int windowWidth) {
    if (roomLabel.empty()) {
        return;
    }
    static constexpr Color kRoomLabelColor{190, 190, 190};
    const int textWidth = frame.text_width(roomLabel, RenderConfig::kRoomLabelFontSize, RenderConfig::kRoomLabelTextThickness);
    const int x = std::max(0, (windowWidth - textWidth) / 2);
    frame.put_text(roomLabel, x, RenderConfig::kRoomLabelYPx, RenderConfig::kRoomLabelFontSize, kRoomLabelColor,
                    RenderConfig::kRoomLabelTextThickness);
}

}  // namespace kungfu
