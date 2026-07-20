#pragma once

#include "IGameView.hpp"
#include "Img/img.hpp"
#include "AssetManager.hpp"
#include "Core/CoordinateMapper.hpp"
#include "Core/PieceAnimator.hpp"

namespace kungfu {

// Everything to do with drawing the chess board itself: the checkerboard +
// coordinate labels, a selection/last-move highlight frame, one piece
// (sprite + its rest-progress ring), and the ring on its own. Kept separate
// from ScoreboardRenderer, which draws the side panels and knows nothing
// about the board, pieces, or chess at all.

// Draws the checkerboard squares and a-h/1-8 coordinate labels onto
// 'target', using 'mapper' for cell placement and board extent - so this
// stays correct for whatever size/offset the mapper was actually built
// with, rather than assuming a fixed board size itself.
void drawCheckerboardAndLabels(Img& target, const CoordinateMapper& mapper);

// Draws a thin frame around one board cell - used for selection / last-move highlighting.
void drawCellOutline(Img& frame, const CoordinateMapper& mapper, int row, int col, const cv::Scalar& color);

// A radial "charging up" meter for any piece-unavailable timer with a known
// remaining/total duration (post-move cooldown or post-jump short rest): a
// dim track ring plus a brighter arc that sweeps clockwise from 12 o'clock
// as the wait elapses (empty right after landing, a full ring once
// selectable again), colored from rust to gold so it reads as "warming up"
// rather than alarming - deliberately distinct from the white selection /
// green last-move outlines so the highlights never get visually confused.
void drawRestRing(Img& frame, const CoordinateMapper& mapper, int cellPx, int cellPy,
                   double remainingMs, double totalMs);

// Draws one piece: its current animation frame at its (possibly mid-move,
// fractional) board position, plus its rest ring if it has one. Looks up
// the right sprite sequence via 'assets' and the right frame via 'animator'.
void drawPiece(Img& frame, const RenderPiece& rp, AssetManager& assets, PieceAnimator& animator,
               const CoordinateMapper& mapper);

// Draws a centered, game-lifecycle banner (e.g. "GAME START", "WHITE WINS") over the
// board area, with a dark backdrop so it stays readable regardless of what's underneath.
void drawBanner(Img& frame, const CoordinateMapper& mapper, const std::string& text);

}  // namespace kungfu
