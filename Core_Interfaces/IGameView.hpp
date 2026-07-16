#pragma once
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
    bool hasSelection = false;
    int selectedRow = 0;
    int selectedCol = 0;

    bool hasLastMove = false;
    int lastMoveFromRow = 0;
    int lastMoveFromCol = 0;
    int lastMoveToRow = 0;
    int lastMoveToCol = 0;
};
} // namespace kungfu

class IGameView {
public:
    virtual ~IGameView() = default;

    virtual void init() = 0;

    // Render current frame. `pieces` are in logical coordinates.
    virtual void render(const std::vector<kungfu::RenderPiece>& pieces,
                         const kungfu::BoardHighlight& highlight) = 0;

    virtual bool isOpen() const = 0;

    // Register the handler that receives raw pixel clicks from this view.
    virtual void setInputHandler(kungfu::IInputHandlerPtr handler) = 0;
};