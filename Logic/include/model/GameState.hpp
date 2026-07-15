#pragma once

namespace kungfu {

// Check/checkmate are intentionally absent: the course spec's common route
// excludes them entirely (win condition is king capture only).
enum class GameState {
    NotStarted,
    Running,
    Paused,
    Finished
};

}  // namespace kungfu
