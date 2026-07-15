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
build\kungfu_tests.exe                          # run all unit tests
build\kungfu_tests.exe "Rule 8: Knight Landing"  # run a single test/section by name
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
- **`Logic/`** — the engine, built as the `kungfu_engine` static library.
  - `board/` (`Board`/`IBoard`) — piece storage and lookup.
  - `pieces/` — `Piece` base class + one subclass per type (`King`, `Queen`, `Rook`, `Bishop`, `Knight`, `Pawn`).
  - `rules/` (`RuleEngine`/`IRuleEngine`) — per-piece-type move validation and pawn promotion checks.
  - `movement/` (`MovementSystem`) — geometry helpers (bounds checks, pawn double-step detection).
  - `collision/` (`CollisionSystem`) — static path-clearance checks for sliding pieces (queen/rook/bishop).
  - `game/` — the orchestration layer:
    - `Game` — owns board/rule engine/collision system and the top-level `selectPiece`/`requestMove`/`requestJump`
      API. Implements `IGameInputTarget`.
    - `GameController` — wraps a `Game` and adds click-based selection state (which cell is currently selected),
      also implementing `IGameInputTarget`. This is the class the UI/input layer should drive.
    - `RealTimeArbiter` — the real-time heart of the game. Tracks in-flight `PendingMove`s and, on each
      `advanceTime(ms)` tick, resolves per-step collisions: friendly pieces block each other, enemy pieces racing
      for the same square are decided by whichever started moving first, and knights are exempt from mid-path
      collisions (only their landing square matters). A captured king ends the game.
    - `UIInputAdapter`/`IGameInputAdapter` — translates raw pixel clicks into board cells and into
      `selectPiece`/`requestMove`/`requestJump` calls against whatever `IGameInputTarget` it's bound to.
  - `BoardParser` — parses a text format (`"Board:"` header, then 8 rows of space-separated tokens like `wP`, `bK`,
    `.`/`_` for empty, optionally followed by a `"Commands:"` section with `PRINT`, `CLICK x y`, `CLICK_CELL r c`,
    `WAIT ms`, `START`, `STOP`, `MOVE r1 c1 r2 c2`) — used both to set up the initial board and to script test
    scenarios end-to-end.
- **`UI/`** — the only OpenCV-dependent code. `UI_OpenCV/OpenCvView` implements `IGameView` using `Img` (a small
  `cv::Mat` wrapper in `UI/Img/`), `AssetManager` (loads/caches each piece's idle sprite from
  `UI/assets/pieces1/<Kind><Color>/states/idle/sprites/1.png`), and `CoordinateMapper` (pixel↔cell conversion only).

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
