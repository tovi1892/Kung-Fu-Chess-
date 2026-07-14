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
    // Remaining cooldown in milliseconds
    double cooldownMs = 0.0;
    // Current state (idle, moving, airborne)
    int state = 0;
};
} // namespace kungfu

class IGameView {
public:
    virtual ~IGameView() = default;

    // Initialize the view and load resources (board and piece images)
    virtual void init() = 0;

    // Render current frame. `pieces` are in logical coordinates.
    virtual void render(const std::vector<kungfu::RenderPiece>& pieces) = 0;

    // Whether the window remains open
    virtual bool isOpen() const = 0;

    // Register the handler that receives raw pixel clicks from this view.
    virtual void setInputHandler(kungfu::IInputHandlerPtr handler) = 0;
};