#pragma once
#include <string>
#include <vector>
#include <memory>
#include "IInputHandler.hpp"

// Lightweight render data structure shared between Logic and UI.
namespace kungfu {
struct RenderPiece {
    // Unique id for the piece (pointer value cast to uintptr_t or explicit id)
    uintptr_t id = 0;
    // Piece type and color for choosing sprite
    int type = 0; // use enums from core when mapping
    int color = 0;
    // Real-valued coordinates in board-space or pixels depending on mapping
    double x = 0.0;
    double y = 0.0;
    // Remaining post-move cooldown, in ms - 0 unless state is Cooldown.
    // Meaningless (0) while moving: a view has no need for a travel-time
    // countdown, only for how long until the piece is selectable again.
    double cooldownMs = 0.0;
    // The full cooldown duration this piece started with (0 unless state is
    // Cooldown) - paired with cooldownMs so a view can compute a progress
    // fraction (e.g. for a fill/wipe indicator) without needing to know
    // GameConfig's base cooldown or the game's speed multiplier itself.
    double cooldownTotalMs = 0.0;
    // Current state (idle, moving, airborne)
    int state = 0;
};

// Cosmetic, cross-boundary hint for which squares a view should highlight -
// purely a presentation concern (selection feedback, last-move feedback),
// carries no gameplay meaning. Rows/cols are plain ints, same convention as
// RenderPiece, so this struct stays free of any Logic model type.
struct BoardHighlight {
    bool hasSelection = false;   // whether a square is currently selected
    int selectedRow = 0;
    int selectedCol = 0;

    bool hasLastMove = false;    // whether a recent move/jump should still be highlighted
    int lastMoveFromRow = 0;
    int lastMoveFromCol = 0;
    int lastMoveToRow = 0;
    int lastMoveToCol = 0;
};

// One entry in a player's move list: when it happened (elapsed game time,
// ms) and its notation (e.g. "e5", "Nxe5"). Plain data, mirrors
// history::MoveRecord without pulling a Logic header across the boundary.
struct MoveEntry {
    double elapsedMs = 0.0;
    std::string notation;
};

// One player's side-panel contents: name, running score, and move list in
// play order (oldest first) - a view decides for itself how many of the
// most recent entries actually fit on screen.
struct PlayerPanel {
    std::string name;
    int score = 0;
    std::vector<MoveEntry> moves;
    // This account's Elo rating - 0 means "not yet known" (e.g. before any match this
    // client is part of has concluded, or for the opponent's panel pre-match).
    int rating = 0;
};

// Both players' panel contents for one frame - purely a presentation
// concern (name/score/move-list display), carries no gameplay meaning.
struct Scoreboard {
    PlayerPanel white;
    PlayerPanel black;
    // The current match's room id, for display - empty means "don't show anything"
    // (quick-match games have no user-facing room id).
    std::string roomLabel;
};

// A transient game-lifecycle message (start/end) a view should display centered over
// the board for a short while - main.cpp owns how long 'visible' stays true. Plain data,
// same reasoning as BoardHighlight: purely presentation, carries no gameplay meaning.
struct Banner {
    bool visible = false;
    std::string text;
};
} // namespace kungfu

class IGameView {
public:
    virtual ~IGameView() = default;

    // Loads resources and opens the window/canvas. Called once, before the render loop starts.
    virtual void init() = 0;

    // Render current frame. `pieces` are in logical coordinates.
    virtual void render(const std::vector<kungfu::RenderPiece>& pieces,
                         const kungfu::BoardHighlight& highlight,
                         const kungfu::Scoreboard& scoreboard,
                         const kungfu::Banner& banner) = 0;

    // False once the window has been closed - the signal that ends the caller's render loop.
    virtual bool isOpen() const = 0;

    // Register the handler that receives raw pixel clicks from this view.
    virtual void setInputHandler(kungfu::IInputHandlerPtr handler) = 0;
};