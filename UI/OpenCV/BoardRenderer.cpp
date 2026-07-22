#include "BoardRenderer.hpp"

#include <algorithm>
#include <string>

#include "RenderConfig.hpp"

namespace kungfu {

void drawCheckerboardAndLabels(Img& target, const CoordinateMapper& mapper, const LayoutScale& scale) {
    const int margin = mapper.margin();
    const int rows = mapper.rows();
    const int cols = mapper.cols();

    static constexpr Color kLightSquare{240, 217, 181};
    static constexpr Color kDarkSquare{181, 136, 99};
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
    static constexpr Color kLabelColor{215, 215, 215};
    const int centeringOffset = scale.px(RenderConfig::kLabelHorizontalCenteringOffsetPx);
    const double labelFontSize = scale.font(RenderConfig::kLabelFontSize);
    const int labelThickness = scale.px(RenderConfig::kLabelTextThickness);
    for (int c = 0; c < cols; ++c) {
        const std::string label(1, static_cast<char>('a' + c));
        const int x = mapper.cellTopLeftX(c) + mapper.cellWidth() / 2 + centeringOffset;
        target.put_text(label, x, mapper.offsetY() + margin + scale.px(RenderConfig::kFileLabelTopOffsetPx),
                         labelFontSize, kLabelColor, labelThickness);
        target.put_text(label, x, mapper.offsetY() + mapper.boardHeight() - margin + scale.px(RenderConfig::kFileLabelBottomOffsetPx),
                         labelFontSize, kLabelColor, labelThickness);
    }
    for (int r = 0; r < rows; ++r) {
        const std::string label = std::to_string(rows - r);
        const int y = mapper.cellTopLeftY(r) + mapper.cellHeight() / 2 + scale.px(RenderConfig::kRankLabelVerticalCenteringOffsetPx);
        target.put_text(label, mapper.offsetX() + margin / 2 + centeringOffset, y, labelFontSize, kLabelColor, labelThickness);
        target.put_text(label, mapper.offsetX() + mapper.boardWidth() - margin / 2 + centeringOffset,
                         y, labelFontSize, kLabelColor, labelThickness);
    }
}

void drawCellOutline(Img& frame, const CoordinateMapper& mapper, const LayoutScale& scale, int row, int col,
                      const Color& color) {
    frame.draw_rect_outline(mapper.cellTopLeftX(col), mapper.cellTopLeftY(row),
                             mapper.cellWidth(), mapper.cellHeight(), color,
                             scale.px(RenderConfig::kHighlightOutlineThickness));
}

namespace {
Color lerpColor(const Color& from, const Color& to, double t) {
    return Color{static_cast<int>(from.r + (to.r - from.r) * t),
                 static_cast<int>(from.g + (to.g - from.g) * t),
                 static_cast<int>(from.b + (to.b - from.b) * t)};
}
}  // namespace

void drawRestRing(Img& frame, const CoordinateMapper& mapper, const LayoutScale& scale, int cellPx, int cellPy,
                   double remainingMs, double totalMs) {
    static constexpr Color kTrackColor{70, 70, 70};
    static constexpr Color kStartColor{170, 90, 20};    // rust - just landed, long wait ahead
    static constexpr Color kReadyColor{255, 210, 40};   // gold - about to become selectable

    const double fraction = std::clamp(1.0 - (remainingMs / totalMs), 0.0, 1.0);
    const int radius = std::min(mapper.cellWidth(), mapper.cellHeight()) / 2 - scale.px(RenderConfig::kRestRingInsetPx);
    const int centerX = cellPx + mapper.cellWidth() / 2;
    const int centerY = cellPy + mapper.cellHeight() / 2;

    frame.draw_arc(centerX, centerY, radius, 0.0, 360.0, kTrackColor, scale.px(RenderConfig::kRestRingTrackThickness));

    // kRestRingStartAngleDeg is an angle, not a pixel/font measure - it never scales.
    const double endAngleDeg = RenderConfig::kRestRingStartAngleDeg + fraction * 360.0;
    frame.draw_arc(centerX, centerY, radius, RenderConfig::kRestRingStartAngleDeg, endAngleDeg,
                    lerpColor(kStartColor, kReadyColor, fraction), scale.px(RenderConfig::kRestRingProgressThickness));
}

void drawPiece(Img& frame, const RenderPiece& rp, AssetManager& assets, PieceAnimator& animator,
               const CoordinateMapper& mapper, const LayoutScale& scale) {
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
        drawRestRing(frame, mapper, scale, px, py, rp.cooldownMs, rp.cooldownTotalMs);
    }
}

void drawBanner(Img& frame, const CoordinateMapper& mapper, const LayoutScale& scale, const std::string& text) {
    static constexpr Color kBackdrop{20, 20, 20};
    static constexpr Color kTextColor{235, 235, 235};

    const double fontSize = scale.font(RenderConfig::kBannerFontSize);
    const int thickness = scale.px(RenderConfig::kBannerTextThickness);
    const int textWidth = frame.text_width(text, fontSize, thickness);

    const int centerX = mapper.offsetX() + mapper.boardWidth() / 2;
    const int centerY = mapper.offsetY() + mapper.boardHeight() / 2;
    const int paddingX = scale.px(RenderConfig::kBannerPaddingXPx);
    const int paddingY = scale.px(RenderConfig::kBannerPaddingYPx);
    const int boxWidth = textWidth + 2 * paddingX;
    const int boxHeight = scale.px(RenderConfig::kBannerTextLineHeightPx) + 2 * paddingY;

    frame.draw_rect(centerX - boxWidth / 2, centerY - boxHeight / 2, boxWidth, boxHeight, kBackdrop);
    frame.put_text(text, centerX - textWidth / 2, centerY + scale.px(RenderConfig::kBannerTextBaselineOffsetPx), fontSize,
                    kTextColor, thickness);
}

}  // namespace kungfu
