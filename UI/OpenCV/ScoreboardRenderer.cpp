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

void drawPlayerPanel(Img& frame, const PlayerPanel& panel, const LayoutScale& scale, int panelX, int panelWidth,
                     int panelHeight) {
    static constexpr Color kNameColor{230, 200, 60};
    static constexpr Color kScoreColor{190, 190, 190};
    static constexpr Color kHeaderColor{225, 225, 225};
    static constexpr Color kMoveColor{205, 205, 205};
    static constexpr Color kDividerColor{90, 90, 90};

    auto centeredX = [&](const std::string& text, double fontSize, int thickness) {
        const int textWidth = frame.text_width(text, fontSize, thickness);
        return panelX + std::max(0, (panelWidth - textWidth) / 2);
    };

    const double nameFontSize = scale.font(RenderConfig::kPanelNameFontSize);
    const int nameThickness = scale.px(RenderConfig::kPanelNameTextThickness);
    const double scoreFontSize = scale.font(RenderConfig::kPanelScoreFontSize);
    const int scoreThickness = scale.px(RenderConfig::kPanelScoreTextThickness);
    const double headerFontSize = scale.font(RenderConfig::kPanelHeaderFontSize);
    const int headerThickness = scale.px(RenderConfig::kPanelHeaderTextThickness);
    const double moveRowFontSize = scale.font(RenderConfig::kPanelMoveRowFontSize);
    const int moveRowThickness = scale.px(RenderConfig::kPanelMoveRowTextThickness);

    // Rating is folded into the same name line ("alice (1215)") rather than its own row -
    // 0 means "not yet known" (pre-login, or the opponent's rating before a match this
    // client is part of has concluded), so it's simply omitted until then.
    std::string name = panel.name.empty() ? std::string("Player") : panel.name;
    if (panel.rating > 0) {
        name += " (" + std::to_string(panel.rating) + ")";
    }
    frame.put_text(name, centeredX(name, nameFontSize, nameThickness), scale.px(RenderConfig::kPanelNameYPx),
                    nameFontSize, kNameColor, nameThickness);

    const std::string scoreText = "Score: " + std::to_string(panel.score);
    frame.put_text(scoreText, centeredX(scoreText, scoreFontSize, scoreThickness), scale.px(RenderConfig::kPanelScoreYPx),
                    scoreFontSize, kScoreColor, scoreThickness);

    const int headerY = scale.px(RenderConfig::kPanelHeaderYPx);
    const int timeColX = panelX + scale.px(RenderConfig::kPanelTimeColumnOffsetPx);
    const int moveColX = panelX + panelWidth / 2 + scale.px(RenderConfig::kPanelMoveColumnOffsetPx);
    frame.put_text("TIME", timeColX, headerY, headerFontSize, kHeaderColor, headerThickness);
    frame.put_text("MOVE", moveColX, headerY, headerFontSize, kHeaderColor, headerThickness);
    frame.draw_rect(panelX + scale.px(RenderConfig::kPanelDividerXInsetPx), headerY + scale.px(RenderConfig::kPanelDividerYOffsetPx),
                     panelWidth - scale.px(RenderConfig::kPanelDividerWidthInsetPx), scale.px(RenderConfig::kPanelDividerThicknessPx),
                     kDividerColor);

    const int rowStartY = headerY + scale.px(RenderConfig::kPanelRowStartYOffsetPx);
    const int rowHeight = scale.px(RenderConfig::kPanelRowHeightPx);
    const int maxRows = std::max(0, (panelHeight - rowStartY - scale.px(RenderConfig::kPanelBottomPaddingPx)) / rowHeight);

    const int total = static_cast<int>(panel.moves.size());
    const int startIndex = std::max(0, total - maxRows);
    int y = rowStartY;
    for (int i = startIndex; i < total; ++i) {
        const auto& entry = panel.moves[i];
        frame.put_text(formatElapsed(entry.elapsedMs), timeColX, y, moveRowFontSize, kMoveColor, moveRowThickness);
        frame.put_text(entry.notation, moveColX, y, moveRowFontSize, kMoveColor, moveRowThickness);
        y += rowHeight;
    }
}

void drawRoomLabel(Img& frame, const std::string& roomLabel, const LayoutScale& scale, int windowWidth) {
    if (roomLabel.empty()) {
        return;
    }
    static constexpr Color kRoomLabelColor{190, 190, 190};
    const double fontSize = scale.font(RenderConfig::kRoomLabelFontSize);
    const int thickness = scale.px(RenderConfig::kRoomLabelTextThickness);
    const int textWidth = frame.text_width(roomLabel, fontSize, thickness);
    const int x = std::max(0, (windowWidth - textWidth) / 2);
    frame.put_text(roomLabel, x, scale.px(RenderConfig::kRoomLabelYPx), fontSize, kRoomLabelColor, thickness);
}

}  // namespace kungfu
