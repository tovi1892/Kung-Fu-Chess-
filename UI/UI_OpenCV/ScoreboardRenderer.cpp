#include "ScoreboardRenderer.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace kungfu {

namespace {

// mm:ss.mmm, matching the reference layout's TIME column.
std::string formatElapsed(double elapsedMs) {
    const int totalMs = static_cast<int>(elapsedMs);
    const int minutes = totalMs / 60000;
    const int seconds = (totalMs / 1000) % 60;
    const int millis = totalMs % 1000;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ':'
        << std::setw(2) << seconds << '.'
        << std::setw(3) << millis;
    return oss.str();
}

}  // namespace

void drawPlayerPanel(Img& frame, const PlayerPanel& panel, int panelX, int panelWidth, int panelHeight) {
    static const cv::Scalar kNameColor(60, 200, 230);
    static const cv::Scalar kScoreColor(190, 190, 190);
    static const cv::Scalar kHeaderColor(225, 225, 225);
    static const cv::Scalar kMoveColor(205, 205, 205);
    static const cv::Scalar kDividerColor(90, 90, 90);

    auto centeredX = [&](const std::string& text, double fontSize, int thickness) {
        const int textWidth = frame.text_width(text, fontSize, thickness);
        return panelX + std::max(0, (panelWidth - textWidth) / 2);
    };

    const std::string name = panel.name.empty() ? std::string("Player") : panel.name;
    frame.put_text(name, centeredX(name, 0.65, 2), 34, 0.65, kNameColor, 2);

    const std::string scoreText = "Score: " + std::to_string(panel.score);
    frame.put_text(scoreText, centeredX(scoreText, 0.45, 1), 60, 0.45, kScoreColor, 1);

    const int headerY = 95;
    const int timeColX = panelX + 18;
    const int moveColX = panelX + panelWidth / 2 + 10;
    frame.put_text("TIME", timeColX, headerY, 0.42, kHeaderColor, 1);
    frame.put_text("MOVE", moveColX, headerY, 0.42, kHeaderColor, 1);
    frame.draw_rect(panelX + 10, headerY + 8, panelWidth - 20, 1, kDividerColor);

    const int rowStartY = headerY + 28;
    const int rowHeight = 20;
    const int maxRows = std::max(0, (panelHeight - rowStartY - 12) / rowHeight);

    const int total = static_cast<int>(panel.moves.size());
    const int startIndex = std::max(0, total - maxRows);
    int y = rowStartY;
    for (int i = startIndex; i < total; ++i) {
        const auto& entry = panel.moves[i];
        frame.put_text(formatElapsed(entry.elapsedMs), timeColX, y, 0.4, kMoveColor, 1);
        frame.put_text(entry.notation, moveColX, y, 0.4, kMoveColor, 1);
        y += rowHeight;
    }
}

}  // namespace kungfu
