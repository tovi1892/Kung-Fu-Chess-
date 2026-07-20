#pragma once

#include "IGameView.hpp"
#include "Img/img.hpp"

namespace kungfu {

// Draws one player's side panel: name + score centered at top, then a
// TIME/MOVE move list below - a self-contained, chess-agnostic "named,
// scored, timestamped list" widget with no knowledge of the board, pieces,
// or chess at all (kept separate from BoardRenderer for exactly that
// reason). Only as many of the most recent moves as fit in the panel's
// remaining height are shown, so the latest move is always visible instead
// of the list silently overflowing off-screen.
void drawPlayerPanel(Img& frame, const PlayerPanel& panel, int panelX, int panelWidth, int panelHeight);

}  // namespace kungfu
