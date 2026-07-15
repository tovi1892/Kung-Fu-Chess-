# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Kung Fu Chess: a real-time chess variant in C++17. There are no turns — either side can act on any piece at any
moment, but moves aren't instant: a move takes `distance * 1000ms` to complete (see `RealTimeArbiter`), and pieces
can collide, race for a square, or get captured mid-flight. It's a learning project (student is building it while
still learning C++), so keep changes minimal, focused, and avoid introducing new third-party dependencies beyond
OpenCV (already a required dependency) — prefer solving problems by writing code over pulling in libraries.

## Build

Requires OpenCV built for MSVC (expected at `C:\opencv\build`, i.e. `OpenCV_DIR=C:\opencv\build`) and a VS2022
toolchain (CMake/Ninja are bundled with VS2022 Community under
`Common7\IDE\CommonExtensions\Microsoft\CMake\...`). Configure/build from a Developer PowerShell/cmd (or after
sourcing `vcvars64.bat`) so `cl.exe` is on `PATH`:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

`cmake --build build` reporting "ninja: no work to do" is not an error — it means nothing changed since the last
build.

### Running the app

The app loads assets via paths relative to the repo root (`UI/assets/board.png`, `UI/assets/pieces1/...`), and
needs the OpenCV DLLs on `PATH` at runtime. Run from the repo root, not from `build/`:

```
set PATH=C:\opencv\build\x64\vc16\bin;%PATH%
build\kungfu_app.exe
```

### Tests

Two Catch2 (v2.13.9, fetched via `FetchContent`) executables are built alongside the app: `kungfu_tests` (unit,
`Logic/tests/unit/*.cpp`) and `kungfu_integration_tests` (`Logic/tests/integration/GameIntegrationTests.cpp`). Also
registered with CTest as `unit_tests` / `integration_tests`.

```
build\kungfu_tests.exe                    # run all unit tests
build\kungfu_tests.exe "[collision]"       # run tests tagged [collision]
ctest --test-dir build
```

`BUILD_TESTS` (default `ON`) toggles building the two test executables.

## Architecture

The project is split into three layers with a strict dependency direction: `UI` depends on `Logic` and
`Core_Interfaces`; `Logic` does not depend on `UI`.

- **`Core_Interfaces/`** — the abstraction boundary that keeps the UI swappable. `IGameView` (render, isOpen,
  setInputHandler) and `IInputHandler` (handleClick, pollEvent) are the only things `main.cpp`/`Logic` should know
  about a concrete UI backend. `RenderPiece` (declared in `IGameView.hpp`) is the lightweight snapshot struct passed
  from `Logic` to any view implementation.
- **`Logic/`** — the engine, built as the `kungfu_engine` static library. The folder layout under `include/`/`src/`
  mirrors the course's layer boundaries; each layer is testable without the layers above it, and dependencies only
  point downward (`input` → `engine` → `rules`/`realtime` → `model`; `io`/`texttests` sit alongside and depend on
  `model` plus whichever of `engine`/`input` they need to drive).
  - `model/` — `Position`, `Piece` (+ one thin identity subclass per type: `King`, `Queen`, `Rook`, `Bishop`,
    `Knight`, `Pawn` — no movement behavior lives here), `Board`/`IBoard`, `GameState`, `Enums`, `GameConfig`.
    Owns board coordinates, piece identity/lifecycle state, and shared constants (including the fixed
    `kCellSizePx`/`kPieceSpeedPxPerSec` from the course spec). Knows nothing about movement rules, rendering, or
    input.
  - `rules/` — `IPieceRules` + one stateless strategy class per piece type (`RookRules`, `BishopRules`,
    `QueenRules`, `KnightRules`, `KingRules`, `PawnRules`, selected via `pieceRulesFor(PieceType)`) computing legal
    destinations from `Board`+`Piece` data only (this is where blocking, capture-eligibility, and the pawn's
    double-step-from-start rule live). `RuleEngine`/`IRuleEngine` is the read-only
    validation service built on top: bounds → empty-source → friendly-destination → `PieceRules` legality, returning
    a `MoveValidation{is_valid, reason}` with a stable machine-readable `reason`.
  - `movement/` (`MovementSystem`) — generic bounds-check helpers (`isInBounds`, `canMoveTo`,
    `pawnDoubleStepMiddle`) shared by a couple of call sites; not a per-piece rules layer.
  - `collision/` (`CollisionSystem`) — a free `isPathClear(board, from, to)` function plus a small class wrapper.
    Used by `PieceRules`' sliding-piece classes for static legality *and* independently by `RealTimeArbiter` for
    live step-by-step collision during real-time motion; these are two different concerns that happen to share the
    same geometry primitive.
  - `realtime/` (`RealTimeArbiter`) — the real-time heart of the game (extra features: simultaneous movement +
    collision between moving pieces). Tracks in-flight `PendingMove`s and, on each `advanceTime(ms)` tick, resolves
    per-step collisions: friendly pieces block each other, enemy pieces racing for the same square are decided by
    whichever started moving first. Knights are handled separately as a single atomic hop resolved only at
    `arrivalTimeMs`: no intermediate square blocks them or gets captured, and they can never land on a
    friendly-occupied square (rejected earlier by `RuleEngine`) — only what's on the landing square at arrival is
    ever inspected. Also resolves pawn promotion on arrival. A captured king ends the game.
  - `engine/` (`GameEngine`) — application-service coordinator. Owns the game-over flag, delegates legality to
    `IRuleEngine`, and starts/advances motions through `RealTimeArbiter`. `requestMove` returns a
    `MoveResult{is_accepted, reason}` ("ok", "game_over", or a `RuleEngine` reason). Does not own click
    interpretation, selection state, or piece-specific movement logic. Also still owns the experimental "jump"
    mechanic (`PieceState::Airborne`, `tryJump`/`resolveJump`/`handleArrivalAtAirbornCell`) — kept as-is, outside
    the graded common/extra route.
  - `input/` (`Controller`) — the *only* place selection state lives. `handleClick(x, y)` maps pixels to a cell
    using the fixed `CELL_SIZE` convention and delegates to `handleCellClick(row, col)`, which implements the
    select/move/jump/cancel-on-outside-click policy against a `GameEngine`. There is no separate input-adapter
    indirection layer — `Controller` receives clicks directly.
  - `io/` — `BoardParser` (text → `Board`; also exposes `parseBoard`/`trim`/`split` for `ScriptRunner`'s use) and
    `BoardPrinter` (`Board` → text, logical occupancy only, never animation position). Depend only on model data —
    not on `Controller`, `RuleEngine`, `RealTimeArbiter`, or the renderer.
  - `texttests/` (`ScriptRunner`) — the command-script harness. Parses a script (`Board:` section, then
    `Commands:` with exactly `click x y` / `wait ms` / `print board`) and drives it through `BoardParser` →
    `Controller` → `GameEngine::wait` → `BoardPrinter`, the same path a real UI would use. Never touches `Board` or
    `GameEngine` internals directly — see `Logic/tests/integration/GameIntegrationTests.cpp` for the script-driven
    integration test suite.
- **`UI/`** — the only OpenCV-dependent code. `UI_OpenCV/OpenCvView` implements `IGameView` using `Img` (a small
  `cv::Mat` wrapper in `UI/Img/`), `AssetManager` (loads/caches each piece's idle sprite from
  `UI/assets/pieces1/<Kind><Color>/states/idle/sprites/1.png`), and `CoordinateMapper` (pixel↔cell conversion for
  the actual windowed app — distinct from `Controller`'s fixed-`CELL_SIZE` mapping, since the real window can be a
  different size than the `CELL_SIZE=100` convention the DSL/tests assume).

### Non-obvious invariants

- **Row/direction convention**: `GameConfig::kWhitePawnStartRow = 1` / `kWhitePawnPromotionRow = 7` and
  `kBlackPawnStartRow = 6` / `kBlackPawnPromotionRow = 0` mean White advances with `row + 1` and Black with
  `row - 1`. This is the opposite of the usual chess-diagram convention of "White at the bottom" — a board laid out
  visually with White at the bottom needs Black's pawns on the low rows, not White's.
- **Include paths**: `Core_Interfaces/` and `UI/` are each added directly as include directories in
  `CMakeLists.txt` (not their parent). Headers under them must be included by their bare relative path from that
  root (e.g. `#include "IGameView.hpp"`, not `#include "Core_Interfaces/IGameView.hpp"`) — the reverse form only
  happens to resolve in some files by accident, via `-I` flag cancellation, and shouldn't be relied on or copied.
- **Frame drawing**: `OpenCvView` loads `board.png` once and clones it (`Img::clone()`) every frame before drawing
  pieces — drawing directly on the cached image would permanently smear pieces across frames since `cv::Mat` copies
  are shallow by default.
