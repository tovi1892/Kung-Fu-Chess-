#include "BoardRenderer.hpp"

#include <algorithm>
#include <string>

namespace kungfu {

void drawCheckerboardAndLabels(Img& target, const CoordinateMapper& mapper) {
    const int margin = mapper.margin();
    const int rows = mapper.rows();
    const int cols = mapper.cols();

    static const cv::Scalar kLightSquare(181, 217, 240);
    static const cv::Scalar kDarkSquare(99, 136, 181);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const bool isLight = (r + c) % 2 == 0;
            target.draw_rect(mapper.cellTopLeftX(c), mapper.cellTopLeftY(r),
                              mapper.cellWidth(), mapper.cellHeight(),
                              isLight ? kLightSquare : kDarkSquare);
        }
    }

    // Labels sit in the margin around the board, which may share a dark
    // panel background rather than the light checkered squares, so they
    // need a light color to stay readable either way.
    static const cv::Scalar kLabelColor(215, 215, 215);
    for (int c = 0; c < cols; ++c) {
        const std::string label(1, static_cast<char>('a' + c));
        const int x = mapper.cellTopLeftX(c) + mapper.cellWidth() / 2 - 5;
        target.put_text(label, x, mapper.offsetY() + margin - 12, 0.5, kLabelColor, 1);
        target.put_text(label, x, mapper.offsetY() + mapper.boardHeight() - margin + 24, 0.5, kLabelColor, 1);
    }
    for (int r = 0; r < rows; ++r) {
        const std::string label = std::to_string(rows - r);
        const int y = mapper.cellTopLeftY(r) + mapper.cellHeight() / 2 + 5;
        target.put_text(label, mapper.offsetX() + margin / 2 - 5, y, 0.5, kLabelColor, 1);
        target.put_text(label, mapper.offsetX() + mapper.boardWidth() - margin / 2 - 5, y, 0.5, kLabelColor, 1);
    }
}

void drawCellOutline(Img& frame, const CoordinateMapper& mapper, int row, int col, const cv::Scalar& color) {
    frame.draw_rect_outline(mapper.cellTopLeftX(col), mapper.cellTopLeftY(row),
                             mapper.cellWidth(), mapper.cellHeight(), color, 3);
}

namespace {
cv::Scalar lerpColor(const cv::Scalar& from, const cv::Scalar& to, double t) {
    return cv::Scalar(from[0] + (to[0] - from[0]) * t,
                       from[1] + (to[1] - from[1]) * t,
                       from[2] + (to[2] - from[2]) * t);
}
}  // namespace

void drawRestRing(Img& frame, const CoordinateMapper& mapper, int cellPx, int cellPy,
                   double remainingMs, double totalMs) {
    static const cv::Scalar kTrackColor(70, 70, 70);
    static const cv::Scalar kStartColor(20, 90, 170);    // rust - just landed, long wait ahead
    static const cv::Scalar kReadyColor(40, 210, 255);   // gold - about to become selectable

    const double fraction = std::clamp(1.0 - (remainingMs / totalMs), 0.0, 1.0);
    const int radius = std::min(mapper.cellWidth(), mapper.cellHeight()) / 2 - 4;
    const int centerX = cellPx + mapper.cellWidth() / 2;
    const int centerY = cellPy + mapper.cellHeight() / 2;

    frame.draw_arc(centerX, centerY, radius, 0.0, 360.0, kTrackColor, 3);

    const double endAngleDeg = -90.0 + fraction * 360.0;
    frame.draw_arc(centerX, centerY, radius, -90.0, endAngleDeg, lerpColor(kStartColor, kReadyColor, fraction), 4);
}

void drawPiece(Img& frame, const RenderPiece& rp, AssetManager& assets, PieceAnimator& animator,
               const CoordinateMapper& mapper) {
    // rp.x/rp.y are already fractional (GameEngine interpolates them while a
    // piece is mid-move) - map them straight to sub-cell pixel position
    // instead of rounding to a whole cell first, or the piece would visibly
    // jump once mid-flight instead of sliding smoothly.
    const int px = static_cast<int>(mapper.cellTopLeftXf(rp.y) + 0.5);
    const int py = static_cast<int>(mapper.cellTopLeftYf(rp.x) + 0.5);

    const auto& animations = assets.getAnimations(static_cast<PieceType>(rp.type),
                                                    static_cast<PlayerColor>(rp.color),
                                                    mapper.cellWidth(), mapper.cellHeight());
    const auto pieceState = static_cast<PieceState>(rp.state);
    const auto& sequence = animations.forState(pieceState);
    if (sequence.frames.empty()) {
        return;
    }

    const int frameIndex = animator.frameIndexFor(rp.id, pieceState, sequence);
    sequence.frames[frameIndex].draw_on(frame, px, py);

    if (rp.cooldownMs > 0 && rp.cooldownTotalMs > 0) {
        drawRestRing(frame, mapper, px, py, rp.cooldownMs, rp.cooldownTotalMs);
    }
}

void drawBanner(Img& frame, const CoordinateMapper& mapper, const std::string& text) {
    static const cv::Scalar kBackdrop(20, 20, 20);
    static const cv::Scalar kTextColor(235, 235, 235);

    const double fontSize = 1.0;
    const int thickness = 2;
    const int textWidth = frame.text_width(text, fontSize, thickness);

    const int centerX = mapper.offsetX() + mapper.boardWidth() / 2;
    const int centerY = mapper.offsetY() + mapper.boardHeight() / 2;
    const int paddingX = 24;
    const int paddingY = 18;
    const int boxWidth = textWidth + 2 * paddingX;
    const int boxHeight = 34 + 2 * paddingY;

    frame.draw_rect(centerX - boxWidth / 2, centerY - boxHeight / 2, boxWidth, boxHeight, kBackdrop);
    frame.put_text(text, centerX - textWidth / 2, centerY + 10, fontSize, kTextColor, thickness);
}

}  // namespace kungfu
