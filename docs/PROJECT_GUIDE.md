# Kung Fu Chess — Full Project Guide

This document explains the entire project from scratch: what it is, how the pieces fit
together, and what every class and function is responsible for and why it exists. It's
written for someone who has never seen the codebase.

A note on scope: "every line" is interpreted as *every class and every function*, with
its purpose and its reasoning — not a literal line-by-line transcript (that would just be
the source code again, with less value). Anywhere you want true line-by-line detail, open
the file at the path given; every section below points at exact files.

---

## 1. What this project is

Kung Fu Chess is a **real-time chess variant**, written in C++17. The rules of how pieces
move are ordinary chess rules (rook slides straight, bishop slides diagonally, pawn moves
forward with diagonal captures, etc.) — but the *turn structure* is gone. In standard
chess, players alternate; here, **both sides can act on any piece at any moment**, and a
move is not instantaneous:

- A move takes real time to complete — `distance × ~140ms` per cell (`GameConfig::kMsPerCell`,
  Logic/include/model/GameConfig.hpp:18) — so a piece is visibly *traveling* across the
  board, not teleporting.
- Because both sides move simultaneously and moves take time, **pieces can collide**: two
  pieces can race for the same square, one can capture another mid-flight, or a piece can
  be blocked by a friendly piece that's still in the way.
- After a piece finishes a move, it goes on a short **cooldown** (`PieceState::Cooldown`)
  before it can be selected again — this is what stops one side from just spamming the
  same piece nonstop.
- There's an experimental **jump** mechanic: click a piece twice to send it briefly
  airborne (`PieceState::Airborne`), immune to capture for that window, landing into a
  brief rest afterward.

The only win condition is capturing the enemy king — there's no check/checkmate concept
(`Enums.hpp:28-29` says this explicitly).

It's built as three physically separate pieces of software glued together by a small,
deliberately generic interface — the next section explains why.

---

## 2. The big picture: three layers, one direction

```
        UI/  (OpenCV window, drawing, mouse input)
         |
         v  (depends on)
Core_Interfaces/  (IGameView, IInputHandler — the ONLY things UI and Logic share)
         ^
         |  (depends on)
        Logic/  (the actual chess-with-time engine — no OpenCV, no windowing, nothing visual)
```

**The rule that makes this work: `Logic/` never depends on `UI/`.** Logic doesn't
`#include` a single OpenCV header, doesn't know a window exists, doesn't know what a
pixel is. This is why, in principle, you could swap `UI/` for a completely different
renderer (a web frontend, a terminal UI, another graphics library) without touching a
single file under `Logic/`.

`Core_Interfaces/` is the tiny seam that makes the swap possible: two abstract
interfaces (`IGameView`, `IInputHandler`) plus a handful of **plain data structs**
(`RenderPiece`, `BoardHighlight`, `Scoreboard`/`PlayerPanel`/`MoveEntry`). Logic builds
these structs and hands them across; UI reads them and draws. Neither side ever passes
the other a real object from its own world — Logic never gets a `cv::Mat`, UI never gets
a `Piece*`.

Inside `Logic/` itself there's a second, finer-grained layering (this is the part the
project's `CLAUDE.md` calls out as fixed and not to be restructured):

```
model  →  rules  →  movement / collision  →  realtime  →  engine  →  input
                                                              ^
                                            io / texttests / history (alongside, depend on what they need)
```

Each layer only depends on the ones below it. `model` knows nothing about legality.
`rules` knows legality but never mutates anything. `realtime` is the only thing that
actually moves pieces around over time. `engine` is the single front door everything
above it talks to. This one-directional dependency chain is what lets you test, say,
`rules/` in complete isolation without spinning up a whole game — and it's why the
"where does X live" question almost always has one unambiguous answer.

---

## 3. A single click, traced end to end

The fastest way to actually understand how the layers connect is to follow one concrete
action all the way through. Say you click a white rook, then click a square three cells
to its right.

1. **OpenCV fires a mouse callback.** `OpenCvView::onMouse` (UI/OpenCV/OpenCvView.cpp:72)
   is registered with `cv::setMouseCallback` in `init()`. It filters for left-button-down
   and forwards the raw pixel `(x, y)` to whatever `IInputHandler` was registered.
2. **`main.cpp`'s `BoardClickHandler::handleClick`** (main.cpp:49) is that handler. It
   converts the pixel to a board cell via `CoordinateMapper::pixelToCell` (UI/Geometry/CoordinateMapper.hpp:30),
   then calls `Controller::handleCellClick(row, col)`.
3. **`Controller::handleCellClick`** (Logic/src/input/Controller.cpp:50) is the actual
   selection state machine — the *only* place in the whole codebase that remembers "a
   square is currently selected." First click: no selection yet, so if the clicked square
   holds a selectable piece (not already `Moving` — see `isSelectable`, Controller.cpp:11),
   it's remembered as `selectedPosition_`. Second click (this example): there *is* a
   selection, so it calls `GameEngine::requestMove(from, to)`.
4. **`GameEngine::requestMove`** (Logic/src/engine/GameEngine.cpp:33) is where legality
   is actually decided. It checks the game isn't over, the source square has a piece, the
   piece isn't already `Moving`/`Airborne` (or queues a **premove** if it's on `Cooldown`/
   `ShortRest` — see below), then asks `IRuleEngine::validateMove(from, to)`.
5. **`RuleEngine::validateMove`** (Logic/src/rules/RuleEngine.cpp:18) checks bounds, that
   the source isn't empty, that the destination isn't blocked by a *stationary* friendly
   piece, and finally asks `pieceRulesFor(PieceType::Rook)` — which returns the shared
   `RookRules` instance — for `legalDestinations(board, piece)`. `RookRules::legalDestinations`
   (Logic/src/rules/PieceRules.cpp:75) walks outward from the rook in all four straight
   directions and collects every reachable square. If the clicked destination is in that
   set, the move is valid.
6. Back in `GameEngine::requestMove`: on success, it records the move's notation into
   `GameRecord` (`record_.recordMove(...)`, GameEngine.cpp:72) and calls
   `RealTimeArbiter::startMove(from, to, pieceId)` (RealTimeArbiter.cpp:122), which computes
   the travel distance/direction/timing and pushes a new `PendingMove`. The piece's state
   flips to `PieceState::Moving`.
7. **Every frame after that**, `main.cpp`'s loop calls `controller->handleTimePassed(16)`
   → `GameEngine::wait(16)` → `RealTimeArbiter::advanceTime(16)` (RealTimeArbiter.cpp:345),
   which ticks the simulated clock millisecond-by-millisecond, moving the rook one board
   cell at a time (`resolveStepMove`, RealTimeArbiter.cpp:240), checking for collisions
   with anything else that happens to be on that square at that exact moment. When the
   rook arrives, it goes onto cooldown (`beginCooldown`) and, if it landed on the
   promotion row (only relevant for pawns), gets promoted.
8. **Also every frame**, `main.cpp` calls `game->getRenderState()` (GameEngine.cpp:116)
   and hands the result to `view.render(...)`. This is what actually gets pixels on
   screen — the next section (and §4.6/§4.7) explain it in detail.

That one round trip — click → `Controller` → `GameEngine` → `RuleEngine`/`PieceRules` →
`RealTimeArbiter` → (every frame) `getRenderState()` → `OpenCvView::render` — is the whole
architecture in miniature. Everything below is detail on each stop along that path.

---

## 4. Layer-by-layer reference

### 4.1 `Core_Interfaces/` — the boundary

**File:** `Core_Interfaces/IGameView.hpp`

- `struct RenderPiece` — one piece's drawable state: `id` (stable per-piece identifier),
  `type`/`color` (as plain `int`, not the Logic enum — deliberately, so this header never
  needs to `#include` a Logic header), `x`/`y` (fractional board coordinates — fractional
  because a piece mid-move needs to be drawn *between* two cells), `state`, and
  `cooldownMs`/`cooldownTotalMs` (remaining/total wait before the piece is selectable
  again — used to draw the "charging up" ring).
- `struct BoardHighlight` — which squares (if any) should be outlined for selection /
  last-move feedback. Purely cosmetic; carries zero gameplay meaning.
- `struct MoveEntry` / `struct PlayerPanel` / `struct Scoreboard` — one move's
  time+notation, one player's full panel (name/score/move list), and both panels
  together — the data behind the side-panel UI.
- `struct Banner` — a transient game-lifecycle message (`visible`, `text`) a view should
  display centered over the board for a short while (game start/end, forfeit, reconnect) —
  the composition root (§4.17) owns how long `visible` stays true. Same reasoning as
  `BoardHighlight`: purely presentation, carries zero gameplay meaning.
- `class IGameView` — the interface any renderer must implement: `init()` (open the
  window once), `render(pieces, highlight, scoreboard, banner)` (draw one frame),
  `isOpen()` (false once the window is closed — what ends the composition root's render
  loop), `setInputHandler(...)`.

**File:** `Core_Interfaces/IInputHandler.hpp`

- `class IInputHandler` — the other direction of the boundary: `handleClick(x, y)` is
  what a concrete view calls when the user clicks, without needing to know what a "cell"
  or a "piece" is — that translation happens on the Logic/main.cpp side.

Every one of these types is plain data or a pure interface — no logic, no OpenCV, no
pointers to real `Piece`/`Board` objects. That's what makes this a genuine boundary
rather than just "some shared headers."

### 4.2 `Logic/model/` — what exists on the board

**`Position.hpp`/`.cpp`** — a `(row, col)` pair. Pure geometry: no bounds checking, no
idea what (if anything) sits on that square. Has convenience predicates used all over the
rules layer: `isDiagonalTo`, `isStraightTo`, `isKnightJumpTo`, plus `operator<` (so
`Position` can be a `std::set`/`std::map` key — this is what lets `legalDestinations`
return a `std::set<Position>`).

**`Enums.hpp`** — every shared enum: `PieceType`, `PlayerColor`, `PieceState` (`Idle,
Moving, Cooldown, Airborne, ShortRest, Captured`), `GameState` (`NotStarted, Running,
Paused, Finished`).

**`GameConfig.hpp`** — every timing/sizing constant, in one place so they can't drift out
of sync: board size, pixel cell size, piece speed, `kMsPerCell` (derived from the two
speeds — the base unit all move timing is built from), cooldown/airborne/short-rest
durations, and each color's pawn start/promotion row. Worth noting explicitly:
**`kWhitePawnStartRow = 1`**, so White advances with `row + 1` — the opposite of the
usual "White at the bottom of the diagram" mental model. A board drawn with White at the
bottom needs Black's pawns on the low rows, not White's.

**`pieces/Piece.hpp`/`.cpp`** — one piece's identity (`id`, `type`, `color`) and
lifecycle state (`position`, `state`). Carries **no movement logic** — see §4.3 for where
that actually lives. `id()` is assigned once at construction and never reused/recycled,
specifically so a piece can be tracked across frames (by `RenderPiece::id`) even while its
position keeps changing. `setPosition`/`setState` are the only mutators, and only ever
called by `Board` (position) or `RealTimeArbiter`/`GameEngine` (state) — never by any
rules code, keeping "legality checking" and "actually changing things" cleanly separated.

**`pieces/King.hpp` … `Pawn.hpp`/`.cpp`** — six one-constructor subclasses of `Piece`
(e.g. `King::King(color, pos) : Piece(PieceType::King, color, pos) {}`). They override
nothing and add no members — see the separate discussion in this conversation ("why a
class per piece") for why these specifically exist (type-safe construction, not
polymorphism) versus the real per-piece *behavior* split in `rules/`.

**`IBoard.hpp`** — the abstract contract for "a board's occupancy": `rows()`/`cols()`,
`pieceAt(pos)`, `placePiece`/`removePiece`/`movePiece`/`replacePiece` (the last is for
pawn promotion — swap the piece at a square for a new one), `pieces()` (every piece, as a
non-owning list). Says nothing about whether a move is *legal* — that's `rules/`'s job,
built on top.

**`Board.hpp`/`.cpp`** — the only `IBoard` implementation: an owning
`std::vector<std::unique_ptr<Piece>>`. Every method is a straightforward linear scan over
that vector (this is a chess board, not a huge dataset — simplicity wins over an index).
One thing worth knowing if you're reading test code: **`Board`'s constructors never touch
`pieces_`**, so a freshly-constructed `Board` is already empty — there's no "clear the
board" step needed anywhere. `getRenderState()` (Board.cpp:96) builds the *raw* half of
each frame's `RenderPiece` list (id/type/color/position/state, straight from the stored
data) — `GameEngine::getRenderState()` (§4.7) is what augments this with real-time
interpolation and cooldown progress, since `Board` has no access to `RealTimeArbiter`.

### 4.3 `Logic/rules/` — where each piece can go

This is where the actual "how does a rook move" logic lives — deliberately **not** in the
`model/pieces/*` classes (see §4.2 and the earlier discussion in this conversation for
why).

**`PieceRules.hpp`/`.cpp`**

- `class IPieceRules` — one method: `legalDestinations(board, piece) → std::set<Position>`.
  Every square the piece could currently be asked to move toward, based only on its own
  position/color/type and the board's *current* occupancy — nothing about anything
  happening in real time.
- `RookRules`/`BishopRules`/`QueenRules` — all built on the shared helper
  `collectSlidingDestinations` (PieceRules.cpp:39): walk outward one direction at a time
  to the board edge, collecting every square that isn't blocked by a *stationary*
  friendly piece. Rook = 4 straight directions, Bishop = 4 diagonal, Queen = all 8.
- `KnightRules`/`KingRules` — built on `collectSteppedDestinations` (PieceRules.cpp:56):
  a fixed list of offsets (the 8 knight-jump shapes, or the 8 adjacent squares) checked
  once each, no walking.
- `PawnRules` — the one genuinely irregular piece: one step forward (never a capture,
  so *any* occupant blocks it), two steps forward only from the pawn's start row, and
  diagonal captures only when an enemy piece is actually there.
- A key rule that's **different from standard chess**, present in every one of the
  functions above via `blocksAsStationaryFriendly` (PieceRules.cpp:24): a square held by
  a friendly piece that is itself currently `Moving` does **not** block a new request —
  because by the time this piece would actually get there, real time will likely have
  moved the blocker out of the way (and if not, `RealTimeArbiter` resolves that
  collision dynamically at the moment it actually happens, not here). This is what makes
  "requesting a move through a piece that's about to vacate" work.
- `pieceRulesFor(PieceType) → const IPieceRules&` (PieceRules.cpp:159) — the single
  factory every caller goes through, returning one of six `static const` shared
  instances (no allocation per call, and every rules object is stateless so sharing is
  safe).

**`IRuleEngine.hpp`** — `struct MoveValidation { bool is_valid; std::string reason; }`
(`reason` is always a **stable, machine-readable string**: `"ok"`, `"outside_board"`,
`"empty_source"`, `"friendly_destination"`, `"illegal_piece_move"` — this is what lets
tests and UI alike branch on *why* a move failed, not just whether). `IRuleEngine` is the
read-only legality service: `validateMove`, `isValidMove` (shorthand), `isPawnPromotion`.

**`RuleEngine.hpp`/`.cpp`** — the only implementation. `validateMove` (RuleEngine.cpp:18)
runs, in order: bounds check → is there actually a piece at `from`? → is `to` blocked by
a *stationary* friendly piece? → finally, is `to` in `pieceRulesFor(type).legalDestinations(...)`?
Each check returns its own specific `reason` the moment it fails, so the caller always
knows exactly which rule rejected the move.

### 4.4 `Logic/movement/` — tiny bounds-check helpers

**`MovementSystem.hpp`/`.cpp`** — a small, mostly-stateless helper class: `isInBounds`
(fixed 8×8, or a given size), `canMoveTo` (in-bounds + not the same square),
`pawnDoubleStepMiddle` (the square a pawn's 2-row opening move skips over — geometry
support for a would-be en-passant rule). It's deliberately *not* a per-piece rules layer
— just shared geometry a couple of call sites (`GameEngine::isPositionInBounds`, tests)
need without duplicating the bounds-check formula.

### 4.5 `Logic/collision/` — static occupancy geometry

**`ICollisionSystem.hpp`/`CollisionSystem.hpp`/`.cpp`**

- The free function `isPathClear(board, from, to)` (CollisionSystem.cpp:5) — steps
  square-by-square between two positions (using unit row/col steps) and checks every
  square strictly in between is empty. This exact same geometry is used two different
  places for two different concerns: statically, inside `PieceRules`' sliding-piece
  helper (well — actually `PieceRules` inlines its own version via
  `collectSlidingDestinations`'s edge-walk; `CollisionSystem`'s copy is the
  independently-tested, documented version of the same idea) and, completely separately,
  by `RealTimeArbiter` doing live step-by-step collision *during* motion.
- `class CollisionSystem` — `findCollision` (what's at the destination), `isCapture`
  (destination holds an enemy), `isFriendlyBlock` (destination holds a friendly piece),
  `isPathClear` (member wrapper around the free function above). Worth knowing: neither
  `PieceRules` nor `RealTimeArbiter` actually route through this class day-to-day — they
  do their own inline occupancy checks, because their specific real-time-aware rules
  (e.g. "a `Moving` friendly piece doesn't block") don't fit this class's simpler,
  static-only contract. `CollisionSystem` exists as the standalone, independently
  unit-tested version of the plain geometry.

### 4.6 `Logic/realtime/` — the real-time heart (`RealTimeArbiter`)

This is the most complex file in the project and the thing that actually makes "Kung Fu
Chess" different from chess. Read `RealTimeArbiter.hpp`/`.cpp` directly for full detail;
here's what each piece does and why.

**Data it tracks:**

- `struct PendingMove` — one piece's move, currently in flight: `from`/`to` (the original
  request), `currentPos` (where it actually is *right now*, which can differ from both if
  it got captured mid-slide or blocked), `startTimeMs`/`arrivalTimeMs`/`nextStepTimeMs`
  (all in the arbiter's own simulated millisecond clock), `rowStep`/`colStep` (unit
  direction), `active` (false once the move has ended for any reason).
- `struct CaptureEvent` — `{capturingColor, capturedType}`, recorded whenever a genuine
  capture happens; `GameEngine::wait` drains these each tick to update `GameRecord`'s
  score, so `RealTimeArbiter` itself never needs to know anything about scoring.
- `struct TimerEntry` — a shared `{pieceId, endTimeMs}` shape used identically for three
  different timers: post-move cooldown, post-jump airborne duration, and post-landing
  short rest. (This unification — `beginRest`/`restRemainingMs`/`resolveRestExpirations`
  — was a deliberate refactor this session: cooldown and short-rest used to be four pairs
  of near-duplicated methods; now they're thin, purpose-named wrappers around one shared
  implementation.)

**Constructor** — takes the board, the rule engine (needed to re-validate a premove the
instant its cooldown/rest ends), and a `speedMultiplier` (defaults to `1.0`) that scales
every one of `msPerCell_`/`cooldownMs_`/`airborneMs_`/`shortRestMs_` uniformly, so
"2x game speed" genuinely means faster movement *and* proportionally faster
cooldowns, not just faster movement.

**`startMove(from, to, pieceId)`** (RealTimeArbiter.cpp:122) — the single source of truth
for move timing math: distance = max of the row/col deltas (so diagonal and straight
moves of the same length take the same time), direction = unit step per axis, arrival
time = `start + distance × msPerCell_`. Pushes a new `PendingMove`. Called both from
`GameEngine::requestMove` (a fresh, immediately-legal move) and internally from
`resolveRestExpirations` (a *premove* becoming eligible once its cooldown ends).

**`setPremove(pieceId, to)`** — a piece on cooldown/short-rest can still be *clicked*;
rather than reject the click outright, `GameEngine::requestMove` queues it here,
deliberately **unvalidated at queue time**. This is intentional: submitting any new
request for the same piece — legal or not — overwrites whatever was previously queued,
which is exactly the mechanism that lets a player *cancel* a premove by deliberately
clicking somewhere illegal.

**`beginAirborne(pieceId)`** — starts the jump timer (`GameEngine::tryJump` is the entry
point that calls this, after flipping the piece to `PieceState::Airborne`).

**The three-timer machinery (`beginCooldown`/`beginShortRest`/`resolveCooldownExpirations`/
`resolveShortRestExpirations`/`cooldownRemainingMs`/`shortRestRemainingMs`)** — thin,
purpose-named wrappers over the shared `beginRest`/`restRemainingMs`/`resolveRestExpirations`
(RealTimeArbiter.cpp:46-96). `resolveRestExpirations` does four things per expired timer,
in order: land the piece back to `Idle` *if it's still in the state this timer expected*
(a counter-kill or other event may have already changed its state — in which case this
timer's expiry is simply skipped, not applied on top); check for a queued premove; if one
exists, re-validate it against current board state (it may have become illegal while
waiting); if still valid, actually start it via `startMove`.

**`resolveAirborneExpirations`** — separate from the cooldown/short-rest pair above
because it has one extra rule: landing from a jump *always* transitions into a
`ShortRest`, never straight to `Idle` — unless the piece already changed state some other
way (a counter-kill happened first).

**`resolveAirborneCounterKill(pm, attacker, airbornePiece)`** — the jump mechanic's
signature interaction: an enemy piece's real-time move steps onto a square currently held
by an airborne piece. Instead of the usual "mover captures whatever's there," the
airborne piece is immune — it survives and lands into a short rest, and the *attacker* is
the one removed. If the attacker happened to be a king, this also ends the game.

**`resolveKnightArrival(pm, currentPiece)`** vs. **`resolveStepMove(pm, currentPiece)`**
(RealTimeArbiter.cpp:196-343) — `advanceTime` dispatches to one or the other per pending
move, based on piece type, because knights and everything else behave fundamentally
differently in real time:

- **Knights** move as a single atomic hop. Nothing happens until `arrivalTimeMs` — no
  intermediate square can block them or capture them, and only whatever's actually
  sitting on the landing square at that exact moment is ever inspected. (They can never
  land on a friendly square at all — `RuleEngine` already rejected that when the move was
  requested.)
- **Everything else** moves one cell per `msPerCell_`, and at *each* step, checks what's
  on the square it's entering:
  - Nothing there → just advance.
  - A friendly piece that's itself stationary → this piece stops in place (its own move
    ends here, un-arrived).
  - A friendly piece that's itself *also actively moving* and happens to be occupying
    that exact square right now → same result (this piece stops), the other keeps going
    undisturbed.
  - An enemy piece that's stationary → capture; this piece's move ends on that square
    (never continues past it, even if the originally requested destination was farther).
  - An enemy piece that's *also actively moving* and the two paths just crossed on this
    square → **whichever piece's step is the one entering the square and finding it
    occupied wins** — not whichever one started moving first. This is the "race for a
    square" rule the project's `CLAUDE.md` calls out explicitly.
  - An enemy piece that's `Airborne` → the counter-kill above, instead of a normal
    capture.

**`advanceTime(ms)`** (RealTimeArbiter.cpp:345) — the actual tick loop, and (after this
session's refactor) intentionally short: for each of the `ms` milliseconds, increment the
simulated clock, resolve every active pending move one step (dispatching per-type as
above, returning early the instant a king is captured — no further moves resolve after
game over), sweep out any moves that just ended, then resolve cooldown/airborne/short-rest
expirations for that instant. `GameEngine::wait(ms)` is the only caller.

**`snapshotPendingMoves()`** — returns a *copy* of the pending-move list, so
`GameEngine::getRenderState()` can read (for interpolation) without any risk of mutating
arbiter-internal state from the render path.

### 4.7 `Logic/engine/` — `GameEngine`, the coordinator

`GameEngine` is the single front door for everything above it (`Controller`,
`ScriptRunner`, tests, `main.cpp`) — nothing outside `Logic/` ever talks to `RuleEngine`
or `RealTimeArbiter` directly.

- **Construction** — three overloads, from "brand-new empty 8×8 board" down to "existing
  board + existing rule engine + explicit speed multiplier." If no rule engine is passed,
  it builds a default `RuleEngine` over the same board. Always constructs its own
  `RealTimeArbiter`.
- **`requestMove(from, to)`** (GameEngine.cpp:33) — the full acceptance pipeline: reject
  if the game isn't `Running`; reject if the source square is empty; reject if the piece
  is already `Moving` or `Airborne`; if it's `Cooldown`/`ShortRest`, **queue a premove**
  instead of rejecting outright (`arbiter_->setPremove`); otherwise, ask `RuleEngine` —
  and only on success, record the move's notation into `GameRecord` and actually start it
  via `RealTimeArbiter::startMove`, flipping the piece to `Moving`. Every rejection path
  returns a `MoveResult{false, reason}` with the same stable `reason` string `RuleEngine`
  produced (plus `GameEngine`'s own extra reasons: `"game_over"`, `"empty_source"`,
  `"piece_already_moving"`, `"piece_airborne"`).
- **`wait(ms)`** (GameEngine.cpp:168) — a no-op unless running; otherwise advances the
  arbiter, drains its capture events into `GameRecord`, and checks
  `arbiter_->isKingCaptured()` to flip the game to `Finished`. This is what `Controller::handleTimePassed`
  calls every frame.
- **`getRenderState()`** (GameEngine.cpp:116) — **this is the function that answers "what
  does the UI actually see every frame."** It starts from `Board::getRenderState()` (raw
  id/type/color/position/state per piece — no timing awareness at all), then, for every
  piece: if it has an active `PendingMove`, linearly interpolate its `x`/`y` between
  `from` and `to` based on elapsed time (this is what makes movement look smooth instead
  of jumping cell-to-cell); otherwise, if it's `Cooldown` or `ShortRest`, fill in
  `cooldownMs`/`cooldownTotalMs` from the arbiter (via the shared `applyRestProgress`
  lambda — both timer kinds report through the same two `RenderPiece` fields, since a
  view only ever needs "how long until this is selectable again," not *why*).
- **`tryJump`/`resolveJump`/`handleArrivalAtAirbornCell`** (GameEngine.cpp:184-239) — the
  experimental jump mechanic. `tryJump` is the real entry point (`Controller` calls it via
  `requestJump` when you click an already-selected piece a second time): rejects if the
  piece is in any state other than `Idle`, otherwise flips it to `Airborne` and starts the
  airborne timer. `resolveJump`/`handleArrivalAtAirbornCell` are **manual hooks kept only
  for direct unit testing** — live gameplay never calls them; the real airborne
  landing/counter-kill path is entirely inside `RealTimeArbiter` (§4.6). This whole
  mechanic is explicitly called out in the code as "outside the graded common/extra
  route" — kept as-is, not part of the core rules.
- **`gameRecord()`** — read-only access to the `GameRecord` instance `GameEngine` owns
  and keeps updated (§4.11).

### 4.8 `Logic/input/` — `Controller`, the click/selection state machine

**`Controller.hpp`/`.cpp`** is the *only* place selection state lives in the whole
codebase.

- **`handleClick(x, y)`** (Controller.cpp:31) — converts a raw pixel to a `(row, col)`
  using the fixed `GameConfig::kCellSizePx` convention, then delegates to
  `handleCellClick`. (Note: this is a *different*, simpler pixel↔cell convention than
  `UI/Geometry/CoordinateMapper` uses for the real windowed app — see §4.12 for why two exist.)
- **`handleCellClick(row, col)`** (Controller.cpp:50) — the actual state machine:
  - Click outside the board while something's selected → clear the selection (cancel).
  - No current selection → select the clicked square, but only if it holds a piece that's
    actually selectable (`isSelectable`, Controller.cpp:11 — anything not currently
    `Moving`; note this deliberately allows selecting a piece that's on `Cooldown` or
    `ShortRest`, since that's exactly how a premove gets queued).
  - Click the *same* square that's already selected → treated as a **jump request**
    (`requestJump`), and the selection clears either way.
  - Click a different friendly, selectable piece → **switch** the selection to it (no
    move attempted).
  - Anything else → forward as a move request to `GameEngine::requestMove`; clear the
    selection only if it was accepted.
- **`handleTimePassed(ms)`** — a pure forward to `GameEngine::wait(ms)`; this is the exact
  call `main.cpp`'s render loop makes every frame to advance real time.
- **`requestMove`/`requestJump`** (public, direct) — bypass the click/selection flow
  entirely; exist for tests that want to issue a move without simulating clicks.
- **`isFriendlyPieceAt`** — used internally by `handleCellClick`'s "switch selection"
  branch; also exposed publicly for tests/other callers.

Controller explicitly does **not** decide chess legality (that's `GameEngine`/`RuleEngine`),
mutate the board, or render anything — it only interprets clicks and tracks one
`std::optional<Position>`.

### 4.9 `Logic/io/` — `BoardParser` / `BoardPrinter`

Both depend only on `model/` data — never on `Controller`, `RuleEngine`,
`RealTimeArbiter`, or `GameEngine`. This is what lets them be reused by both `main.cpp`
(building the real starting position) and `ScriptRunner`/tests (building arbitrary test
boards) without pulling in the rest of the engine.

**`BoardParser.hpp`/`.cpp`** — parses a small text DSL into a `Board`:
```
Board:
wR wN wB wQ wK wB wN wR
wP wP wP wP wP wP wP wP
.  .  .  .  .  .  .  .
...
```
- `trim`/`split` — tiny string utilities, exposed as `static` so `ScriptRunner` can reuse
  them for its own line-splitting without duplicating the logic.
- `createPieceFromToken` — decodes a two-character token like `"wR"` into
  `(PlayerColor::White, PieceType::Rook)` via a small `unordered_map` of factory lambdas,
  one per letter (`K`/`Q`/`R`/`B`/`N`/`P`), each constructing the matching `model/pieces/*`
  subclass.
- `parseBoard(lines, ...)` — the row-count/column-count comes from the *first* line's
  token count, and every subsequent row must match it exactly (`"ERROR ROW_WIDTH_MISMATCH"`
  otherwise) — this is what lets `ScriptRunner`'s tests use non-8×8 boards.
- `parseInput(istream, ...)` — the convenience entry point `main.cpp` actually calls:
  scans for a `"Board:"` line, reads rows until `"Commands:"` or EOF, then delegates to
  `parseBoard`.

**`BoardPrinter.hpp`/`.cpp`** — the inverse: `print(out, board, rows, cols)` writes one
space-separated row per line (`"wK"`, `.` for empty). This is the **entire assertion
mechanism** the text-script test DSL uses — a script's "expected output" is literally
what this function produces. It only ever prints *logical* occupancy (from `Board`
data), never animation/pixel position — there's no concept of "mid-move" in this text
representation at all.

### 4.10 `Logic/texttests/` — `ScriptRunner`

**`ScriptRunner.hpp`/`.cpp`** is the command-script test harness — see
`Logic/tests/integration/GameIntegrationTests.cpp` for the actual test suite built on it.
It parses a script with a `Board:` section (handed to `BoardParser::parseBoard`) followed
by a `Commands:` section of exactly three command shapes: `click x y`, `wait ms`,
`print board`. Critically, **it never touches `Board` or `GameEngine` internals
directly** — every command is dispatched through the exact same path a real UI would use:
`Controller::handleClick` for clicks, `GameEngine::wait` for time, `BoardPrinter::print`
for output. This means a passing script test is genuine evidence the *whole* stack
(`Controller` → `GameEngine` → `RuleEngine` → `RealTimeArbiter` → `Board`) works
together, not just one class in isolation.

### 4.11 `Logic/history/` — `GameRecord`

**`GameRecord.hpp`/`.cpp`** — a pure ledger: it accumulates whatever `GameEngine` reports
(accepted moves, resolved captures) and knows nothing about board state or real-time
timing itself.

- `pieceValue(PieceType)` — the scoring table: knight/bishop = 3, rook = 5, queen = 9,
  pawn = 1, king = 0 (a captured king ends the game before score would matter, but the
  function stays total/exhaustive over the enum regardless).
- `moveNotation(type, from, to, isCapture)` — builds a short algebraic-style string:
  just the destination for a pawn move (`"e5"`), a piece letter + destination for
  anything else (`"Ke7"`), with an `"x"` inserted (and, for a pawn, its origin file
  prepended, e.g. `"exd5"`) on a capture. Built from the *requested* move at the moment
  `GameEngine::requestMove` accepts it — not wherever real-time resolution actually ends
  up stopping it — a deliberate simplification, not a full PGN engine.
- `class GameRecord` — `recordMove`/`recordCapture` (mutators, called only by
  `GameEngine`), `movesFor(color)`/`scoreFor(color)` (read accessors, used by
  `main.cpp`'s `buildScoreboard` to populate the UI side panels).

### 4.12 `UI/Geometry/` — `CoordinateMapper`

**`CoordinateMapper.hpp`** — converts between pixels (in the real, windowed app) and
board cells, accounting for a margin (space for the a-h/1-8 labels) and an
`offsetX`/`offsetY` (so the board can sit to the right of a side panel rather than at the
window's origin). `pixelToCell`, `cellTopLeftX/Y` (integer, for whole-cell placement) and
`cellTopLeftXf/Yf` (fractional — used for a piece mid-move, since rounding to a whole cell
first would make it visibly jump instead of sliding). This is a **different, separate**
mapper from `Controller::handleClick`'s fixed-`kCellSizePx` convention (§4.8) — the real
app's window can be a different size than the `CELL_SIZE=100` convention the text-DSL/tests
assume, so they can't share one implementation. `main.cpp` deliberately gets its mapper
from `OpenCvView::mapper()` (a read-only accessor) rather than constructing a second one
itself, specifically so drawing and click-handling can never drift out of sync.

This is the one folder under `UI/` with zero OpenCV (or Win32) dependency — pure
pixel↔cell arithmetic, which is exactly why it's split out on its own rather than sitting
alongside the OpenCV-dependent files in §4.14.

### 4.13 `UI/Img/` — `Img`

**`img.hpp`/`.cpp`** — a thin wrapper around `cv::Mat`; **the one class in the project
allowed to call OpenCV pixel-drawing functions directly** (everything else in `UI/`
draws through this). Notable methods:

- `read(path, size, keepAspect, interpolation)` — load + optionally resize.
- `keyOutNearWhite(threshold)` — the asset files are plain RGB PNGs with a baked-in white
  background (no real alpha channel). This converts to BGRA and makes every near-white
  pixel (all three channels ≥ `threshold`, default 240) fully transparent — a no-op for
  an image that already has real alpha, since existing transparency is trusted rather than
  overwritten by a guess. Called once per frame at *load* time (cached afterward by
  `AssetManager`), not per render.
- `draw_on(target, x, y)` — if this image has a real alpha channel, always alpha-blends
  (manual per-channel `src·α + dst·(1-α)` math) regardless of the target's channel count,
  so a transparent sprite never gets flattened into an opaque box; otherwise a plain copy.
- `draw_rect`/`draw_rect_outline`/`draw_arc`/`put_text`/`text_width` — the other drawing
  primitives everything in `OpenCV/` uses instead of touching `cv::` directly.
- `clone()` — a genuine deep copy. This matters because `cv::Mat` copies are shallow by
  default; `OpenCvView` clones its cached static background every frame before drawing on
  it, or every frame's pieces would permanently smear onto every later frame.

### 4.14 `UI/OpenCV/` — the concrete renderer

Everything here genuinely depends on OpenCV (directly, or via `Img`) — unlike
`Geometry/`, this folder makes no claim of being backend-agnostic.

**`SpriteSequence.hpp`/`.cpp`** — `struct SpriteSequence` (an ordered `vector<Img>` of
frames + `framesPerSec` + `isLoop`, taken straight from one `config.json`) and
`struct PieceAnimationSet` (all five states' sequences for one piece type+color:
`idle`/`move`/`jump`/`longRest`/`shortRest`), with `forState(PieceState)` mapping the
gameplay state to the right sequence (`Moving→move`, `Airborne→jump`, `Cooldown→longRest`,
`ShortRest→shortRest`, everything else→`idle`). Lives here rather than in `Geometry/`
because `SpriteSequence::frames` is a `vector<Img>` — it's exactly as OpenCV-dependent as
`AssetManager` below, so a folder split that claimed otherwise wouldn't have been accurate.

**`PieceAnimator.hpp`/`.cpp`** — tracks, per piece id, how long it's been in its current
state using a **real wall-clock** (`std::chrono::steady_clock`), completely independent of
the engine's simulated game time — animation frame rate is a real display rate, not tied
to game speed. `frameIndexFor(pieceId, state, sequence)`: if the piece's state changed
since last call, resets its clock; otherwise computes `elapsed × framesPerSec`, wrapping
(modulo) if the sequence loops, or clamping to the last frame if it doesn't.

**`AssetManager.hpp`/`.cpp`** — loads and caches every piece's five animation states from
disk. `folderName(type, color)` maps to the asset directory naming convention
(`<Kind><Color>`, e.g. `"PW"` = white pawn). `loadSequence` reads one state's
`config.json` via a **tiny hand-written field extractor** (`extractNumberField`/
`extractBoolField`, AssetManager.cpp:22-38) — deliberately not a general JSON parser,
since the schema is fixed and tiny (just `frames_per_sec` and `is_loop`), matching the
project's "no new third-party dependencies" constraint — then loads `sprites/1.png`,
`2.png`, … until a file is missing, running each through `keyOutNearWhite`.
`getAnimations(type, color, width, height)` is cached per type+color — the whole
five-state, several-frames-each load only happens once per piece kind, not per frame.

**`BoardRenderer.hpp`/`.cpp`** — everything to do with drawing the board itself
(chess-specific), separated from the chess-agnostic side panels below:

- `drawCheckerboardAndLabels(target, mapper)` — the checkerboard squares plus a-h/1-8
  labels, deriving `rows`/`cols`/board extent entirely from the `CoordinateMapper` passed
  in (not a hardcoded board size), so it stays correct for whatever size/offset that
  mapper was actually built with.
- `drawCellOutline` — a thin rectangle outline for selection/last-move highlighting.
- `drawRestRing(frame, mapper, cellPx, cellPy, remainingMs, totalMs)` — the radial
  "charging up" cooldown/short-rest indicator: a dim full-circle track plus a brighter arc
  sweeping clockwise from 12 o'clock as the wait elapses, colored rust→gold
  (`lerpColor`) so it reads as "warming up," deliberately distinct from the white
  selection / green last-move outline colors.
- `drawPiece(frame, rp, assets, animator, mapper)` — maps the piece's fractional
  `x`/`y` straight to a sub-cell pixel position (no rounding — this is what makes
  mid-move motion look smooth), looks up the right `SpriteSequence` via
  `AssetManager::getAnimations(...).forState(...)`, picks the right frame via
  `PieceAnimator::frameIndexFor`, draws it, and draws the rest ring on top if the piece
  currently has cooldown/short-rest progress to show.

**`ScoreboardRenderer.hpp`/`.cpp`** — `drawPlayerPanel(frame, panel, panelX, panelWidth,
panelHeight)`: name + score centered at top, then a `TIME`/`MOVE` two-column move list
below, showing only as many of the most recent moves as actually fit the panel's
remaining height (so the latest move is always visible rather than the list silently
overflowing off-screen). `formatElapsed` turns a millisecond count into `mm:ss.mmm`. This
file is **completely chess-agnostic** — it has no idea what a board, a piece, or a chess
move even is; it just draws a named, scored, timestamped list. That's precisely why it's
a separate file from `BoardRenderer` — see §6 for the full rationale.

**`OpenCvView.hpp`/`.cpp`** — the only `IGameView` implementation, and now (after this
session's refactor) a thin ~80-line orchestrator over the two renderer files above:

- Constructor computes the window layout: `[left panel][board][right panel]`, and builds
  its one `CoordinateMapper` up front.
- `init()` — builds the static background (`drawStaticBackground`: panel background color
  + delegating to `drawCheckerboardAndLabels` + the two panel-divider lines) once, opens
  the OpenCV window, registers the mouse callback.
- `render(pieces, highlight, scoreboard, banner)` — **clones** the cached static
  background (see §4.13's note on why), draws both player panels, draws the
  selection/last-move outlines if present, draws every piece, draws the banner if visible
  (`Banner`, `Core_Interfaces/IGameView.hpp` — start/end-of-game and forfeit/reconnect
  text), then shows the frame and polls for the Escape key / window-closed to set
  `closed_`.
- `onMouse` — the static callback OpenCV invokes; forwards left-clicks to whatever
  `IInputHandler` was registered.
- `mapper()` — read-only accessor the composition root (§4.17) uses instead of building a
  second mapper.

### 4.15 `UI/Windows/` — the Win32 login/room/play screens

Three small native (non-OpenCV) windows, shown in sequence before the OpenCV game window
ever opens, plus a shared theme they all paint with. Everything here is deliberately
**Network-agnostic** — none of these classes talk to `RemoteGameProxy` (§4.16) or know a
`net::` type exists; the composition root (§4.17) is the only place that bridges what a
screen returns into an actual network call. Same boundary discipline as
`Core_Interfaces/` (§4.1): a screen just collects a choice and hands it back as plain data.

**`ChessTheme.hpp`/`.cpp`** — one shared color palette (as `Color` — already
OpenCV-free, so the same values could in principle be reused by the OpenCV-rendered board
too), cached `HFONT`s, and paint helpers (`paintStatic`/`paintEdit`/`drawButton`) every
screen below calls from its own `WM_CTLCOLORSTATIC`/`WM_CTLCOLOREDIT`/`WM_DRAWITEM`
handler. One responsibility — how the flow *looks* — screens still own their own window
procs and message loops.

**`LoginScreen.hpp`/`.cpp`** — the first screen: username + masked password field, one
Sign In button. `show(errorMessage, prefillUsername)` blocks until Sign In is clicked
(returns `{username, password}`, both sanitized) or the window closes (`std::nullopt`,
treated as "quit the app"). `errorMessage` re-displays a `LOGIN_FAIL` reason on retry;
`prefillUsername` re-offers the last-typed username (only the password was wrong) and
focuses the password field directly when set.

**`LoginResultScreen.hpp`/`.cpp`** — a brief "Welcome back, alice (1200)" / "Account
created for alice (1200)" confirmation, shown once `LOGIN_OK` actually arrives. Split out
of `LoginScreen` (which only collects credentials) because it's a genuinely separate
window that merely happens to run right after — the two share nothing but a `sanitize()`
helper `LoginScreen` alone still needs.

**`RoomChoiceScreen.hpp`/`.cpp`** — the second screen: three mutually exclusive radio
choices (Quick Match / Create Room / Join Room, the last with its own room-id field), a
Next button, a Log Out button. `show(welcomeText)` returns a `Result{Outcome, RoomChoice}`
— `Chosen` (with the picked `RoomChoice`), `LogOut` (tears down the current connection and
returns to `LoginScreen`), or `Cancelled` (quit).

**`PlayConfirmScreen.hpp`/`.cpp`** — the third and final screen: shows the previous
screen's choice read-only, plus a Play button that's the *only* thing that actually
triggers the network join. `show(choice, onPlay, pollJoin)` stays Network-agnostic by
taking two callbacks instead of a `RemoteGameProxy&`: `onPlay` fires once, the instant
Play is clicked (the composition root's hook to call `RemoteGameProxy::join(...)`); after
that it switches to a disabled "Connecting..."/"Searching for opponent..." display and
polls `pollJoin()` repeatedly — `Succeeded` resolves `show()` with `Outcome::Play`,
`Failed` shows the returned text and reverts to the interactive Play/Back state (rather
than exiting) so the player can retry. Internally alternates between a blocking
`GetMessage` loop (normal) and a `PeekMessage`-polling loop (while a join is in flight, so
`pollJoin()` gets called every ~50ms without blocking the window).

**A real bug fixed this session, worth knowing about if you're reading these files**:
`LoginScreen`/`LoginResultScreen`/`RoomChoiceScreen`/`PlayConfirmScreen`'s `WM_DESTROY`
handlers deliberately call **no** `PostQuitMessage` — each screen's own message loop exits
via its own local `done` flag, not `WM_QUIT`. `PostQuitMessage` posts to the *thread's*
message queue, and since all four screens run on the same thread in sequence, a stray
posted `WM_QUIT` would sit unconsumed after one screen's loop already stopped via `done`,
and poison the *next* screen's very first `GetMessage()` call — making it return
immediately without ever pumping that new window's messages. Every one of these
`WM_DESTROY` handlers has an explanatory comment instead of the more "obvious"
`PostQuitMessage(0)` specifically because of this.

### 4.16 `Client/` — the network stand-in for a local `GameEngine`

**`RemoteGameProxy.hpp`/`.cpp`** is the client-side mirror of a local `GameEngine`: it
deliberately exposes the same shape (`onMoveStarted`/`onPieceCaptured`/`onGameStarted`/
`onGameEnded`/`getRenderState`) so the composition root's subscriber-wiring code barely
changes from what a local-`GameEngine` version would look like — `game->` becomes
`proxy->` and that's nearly the whole diff. Construction immediately sends
`LOGIN <username> <password>` the instant the underlying `WsClientTransport` connection
opens; it does **not** send `JOIN` itself — `join(mode, room)` is a separate, explicit call
the composition root makes once a room/match choice is actually confirmed (§4.17), so a
connection can sit authenticated-but-unrouted for as long as the login/room-choice screens
need.

**Thread-safety** is the interesting part of this class: `WsClientTransport::onMessage`
fires `handleMessage` on IXWebSocket's own background I/O thread. Plain state (the render
snapshot, `players()`, session flags like `hasRole()`/`myColor()`) is protected by three
small purpose-scoped mutexes (`stateMutex_`/`playersMutex_`/`sessionMutex_`) and can be
read directly from the main thread at any time. The **discrete bus events** (`MoveStarted`,
`PieceCaptured`, `GameStarted`, `GameEnded`, `ForfeitMessage`, `ReconnectedMessage`) are
different: publishing them immediately from the network thread would run every subscriber
(`SoundPlayer`, the score/move-log panels, the banner) on that thread too, racing the main
thread's render loop touching the same data. Instead, `handleMessage` just pushes them onto
`pendingEvents_` (behind `queueMutex_`), and they're only actually published from
`pollEvents()` — which the composition root's render loop calls exactly once per frame, on
the main thread — so every subscriber still only ever runs on the one thread it always has.

**`Network/WsClientTransport.hpp`/`.cpp`** is the client-side counterpart to
`WsServerTransport` (§5.9) — the one place allowed to touch `ix::` directly on the client.
Simpler than its server-side counterpart: the client has no shared mutable *game* state of
its own to protect at the transport layer (`RemoteGameProxy` only ever appends to its own
`EventBus`es or overwrites its own latest-state snapshot), so there's no
`gameMutex`-equivalent here at all.

### 4.17 `main.cpp` — the composition root

The client-side composition root — the one file allowed to know about `Logic/`, `UI/`
(both `OpenCV/` and `Windows/`), and `Client/` all at once. (This replaces an earlier,
pre-networking version of `main.cpp` that built a local `GameEngine` directly and never
talked to a server at all — §5 documents the server-side half of what replaced it.)

Split into two functions plus a thin `main()`, deliberately separated at the point where a
match is actually confirmed:

- **`establishConnection(logger, serverUrl, proxyOpt)`** — drives `LoginScreen` → `LOGIN`
  → `RoomChoiceScreen` → `PlayConfirmScreen` → `JOIN`, in two nested retry loops (outer:
  re-show `LoginScreen` on a wrong password or an explicit Log Out; inner: re-show
  `RoomChoiceScreen` alone on a quick-match timeout or a Back from `PlayConfirmScreen`,
  without forcing a fresh login). Returns `ConnectionOutcome::Matched` once a match is
  confirmed, or `Exit` if the user quit at any screen. `proxyOpt` (a
  `std::optional<RemoteGameProxy>&`) is emplaced into directly on each login attempt
  rather than constructed locally and returned by value — `RemoteGameProxy`'s constructor
  registers callbacks that capture `this`, so moving or copying it after construction
  would leave those callbacks pointing at a stale address.
- **`runGameSession(logger, proxy)`** — everything from "Connected as White/Black/
  spectator" onward: builds the `OpenCvView` + `BoardClickHandler`, wires all six of
  `RemoteGameProxy`'s event buses to scoreboard/banner/sound side effects, and runs the
  render loop (`proxy.pollEvents()` → refresh scoreboard/ratings from the proxy → build
  this frame's `BoardHighlight`/`Banner` → `view.render(...)`) until the window closes.
- **`main(argc, argv)`** itself is now just: construct `logger`/`serverUrl`, call
  `establishConnection`, return early on `Exit`, otherwise call `runGameSession`. An
  optional command-line argument overrides the server host (default `127.0.0.1`).

Other pieces still living in this file's anonymous namespace:

- **`class BoardClickHandler : public IInputHandler`** — forwards raw pixel clicks to the
  server via `RemoteGameProxy::sendClick`, and keeps a purely cosmetic, optimistic local
  echo of the select/deselect pattern for the selection highlight (`localSelection()`) —
  the server remains the sole authority on whether a selection is actually legal; this
  only ever affects what's drawn, never what's sent. Reads `view.mapper()` fresh on every
  click (not a cached copy) since `OpenCvView` rebuilds its mapper on every resize.
- **`template <typename T> class Expiring`** — a tiny (value, expiry) holder used
  identically for both the last-move highlight and the start/end/forfeit/reconnect banner
  text, so `runGameSession` doesn't duplicate the "show this for N milliseconds" logic
  twice.
- **`toJoinMode(RoomChoice::Mode)`** — the one place that maps `RoomChoiceScreen`'s
  UI-facing `RoomChoice::Mode` onto the wire protocol's `net::JoinMode`, since
  `RoomChoiceScreen` itself stays Network-agnostic.
- **`panelFor(scoreboard, color)`** — the small `White`/`Black` → `scoreboard.white`/
  `.black` lookup used throughout `runGameSession`.

---

## 5. `Server/` — the authoritative multiplayer backend

Everything in this section builds into a separate executable (`kungfu_server`, from
`Server/*.cpp` + `kungfu_logic` + `kungfu_network`) from the client (`kungfu_app`, from
`main.cpp` + `kungfu_ui` + `kungfu_client`, which itself links `kungfu_logic` +
`kungfu_network`) — see the root `CMakeLists.txt`. The server owns one real `GameEngine`
per match; every connected client is a thin `RemoteGameProxy`
(`Client/RemoteGameProxy.hpp`/`.cpp`) that talks to it over a small WebSocket text protocol
(`Network/Protocol.hpp`/`.cpp`) instead of holding a `GameEngine` of its own. **This section
documents the server side only** — `Client/RemoteGameProxy` and the login/room/play UI
screens (`UI/Windows/*Screen.*`) are documented in §4.15-§4.17 instead.

### 5.1 `Server/main.cpp` — the composition root

Now genuinely minimal: construct every long-lived object in dependency order (`AccountStore`
→ `Logger` → `ConnectionSessions` → `RoomManager` → `WsServerTransport` → `Outbox`, each only
ever depending on ones already constructed above it), construct `GameServer` with references
to all of them, call `gameServer.run()`. Zero conditional logic, zero protocol knowledge,
zero room/game logic — `std::mutex gameMutex` is declared here (since `Outbox` and
`GameServer` both need a reference to the same instance) but nothing in `main()` itself ever
locks it; see §5.2 for what actually does.

### 5.2 `Server/GameServer.hpp`/`.cpp` — the application layer

This is where the real orchestration policy that used to live in `main()` now lives, and
it's a genuine Ports & Adapters (hexagonal) split, not just a relocation: `WsServerTransport`
(§5.9) is the **adapter** — it knows only connection ids and raw text, nothing about rooms,
accounts, or game logic, and stays that way. `RoomManager`/`Outbox`/`ConnectionSessions`/
`MatchResult`/`ClickRouter` (§5.3-§5.7) are the **domain** — they know nothing about
WebSockets. `GameServer` is the **application layer**: the one class allowed to know about
both sides, whose entire job is translating transport events (a connection opening, a
message arriving, a connection dropping, a tick elapsing) into domain calls.

`run()` registers `onConnect`/`onMessage`/`onDisconnect` on the transport (each a one-line
lambda delegating to a named method below), calls `server.start()`, then owns the 16ms
(`kTickMs`) tick loop directly (replacing what used to be `main()`'s own `while(true)`).

**`gameMutex` lives in `main()` but is used entirely here**: `WsServerTransport` (§5.9)
delivers `onConnect`/`onMessage`/`onDisconnect` on IXWebSocket's own background I/O
thread(s), while every `GameEngine`/`Controller` was built assuming single-threaded access
— so every one of `GameServer`'s three transport-callback handlers, plus `tick()`, takes
`std::lock_guard<std::mutex> lock(gameMutex_)` for its entire body, releasing it before
flushing `Outbox`. `GameServer` holds no lock of its own beyond that — every method on
`RoomManager`/`ConnectionSessions`/`Outbox` explicitly documents that it assumes the caller
already holds `gameMutex`, the same contract, unchanged by this extraction.

Internally, `GameServer` is deliberately **not** one giant method — each handler is broken
down along the actual logical structure of what it decides:

- `handleConnect(id)` — marks the connection pending, logs it.
- `handleMessage(id, text)` — decodes the message, then dispatches to exactly one of three
  mutually exclusive branches (mirroring the original `do { ... break; }` structure, now as
  an `if`/`else if`/`else`): `handleLogin` (still pending), `handleJoin` (authenticated but
  not yet routed), or `ClickRouter::handleClick` (already routed — a no-op otherwise).
- `handleLogin(id, decoded)` — the LOGIN branch: calls `AccountStore::login`, either
  `sessions.authenticate(...)` + `LOGIN_OK`, or `LOGIN_FAIL`.
- `handleJoin(id, decoded, username, rating)` — the JOIN branch, itself dispatching to
  `tryReconnect` (checked first — an active pending-forfeit seat always wins over whatever
  was actually clicked), then `handleQuickMatch` or `handleRoomJoin` depending on
  `join->mode`.
- `tryReconnect`/`handleQuickMatch`/`handleRoomJoin` — exactly the three outcomes JOIN
  handling always had (reconnection, quick-match pairing, create/join/spectate a named
  room), each now independently readable instead of three `break`-separated blocks in one
  function.
- `handleDisconnect(id)` — starts a forfeit countdown if the disconnect happened mid-game,
  otherwise drops the seat immediately; unconditionally forgets the connection's routing and
  session state either way.
- `tick()` — reaps expired quick-match waiters, then calls `advanceRoom` once per room.
- `advanceRoom(room, now)` — **one room's** per-tick work: resolve an expired forfeit if any,
  advance that same room's `GameEngine` by one tick, broadcast `STATE`. Deliberately still
  one room at a time, inline in `tick()`'s loop, not batched — `resolveForfeitIfExpired`'s
  `game->stop()` must run before that *same room's* `game->wait(kTickMs)` in the *same*
  tick, exactly as it did before this class existed (see §5.5's note on why).
- `registerRoomBroadcasts(room)` — the callback passed to `RoomManager::getOrCreateRoom`'s
  `onCreated` hook, wiring a fresh room's `GameEngine` event buses to `Outbox`.

### 5.3 `Server/Outbox.hpp`/`.cpp` — the lock-safe send queue

The one piece of real plumbing, and the reason the other four classes below don't need to
think about networking at all. `enqueue(id, text)`/`enqueueToRoom(room, text)` just buffer a
`(ConnectionId, text)` pair — assumed to be called with `gameMutex` already held. `flush()`
is the opposite: called with the lock **not** held, it briefly re-locks the *same*
`gameMutex` instance (given at construction, alongside the `WsServerTransport&`) just long
enough to swap the buffered vector out, then calls `WsServerTransport::send` for each
message with no lock held at all.

This two-phase shape exists because of a real crash: `ix::WebSocket::send()` can, if it
detects a dead connection, synchronously invoke that same transport's Close callback on the
*same* thread — which tries to re-lock `gameMutex` inside `onDisconnect`, deadlocking against
itself. Queuing while locked and sending only after unlocking is what makes that impossible.
(Explicitly considered and rejected: giving `Outbox` its own private mutex instead of reusing
`gameMutex` — that would be a real, if small, behavioral change, introducing a new
interleaving possibility that doesn't exist today.)

### 5.4 `Server/ConnectionSessions.hpp`/`.cpp` — the auth state machine

Tracks exactly one thing: a connection's identity transition from *pending* (opened, hasn't
sent a valid `LOGIN` yet) to *authenticated* (`markPending`/`isPending`/`authenticate`/
`find`/`forget`). It knows nothing about `AccountStore`, rooms, or the wire protocol —
`GameServer::handleLogin` (§5.2) still owns calling `AccountStore::login(...)` and sending
`LOGIN_OK`/`LOGIN_FAIL`; this class only remembers the result so later code
(`GameServer::handleMessage`'s JOIN-gating check, `GameServer::handleDisconnect`) can answer
"who is this" without re-deriving it. `forget(id)` erases both the pending and authenticated
cases in one call, regardless of which (if either) applied — deliberately unconditional, so
no code path can leave a stale entry behind.

### 5.5 `Server/RoomManager.hpp`/`.cpp` — the room/matchmaking authority

The largest of the five, and the one that grew in stages (originally just room storage +
quick-match pairing; reconnection matching, connection routing, and the forfeit-grace
lifecycle were folded in one at a time afterward, each independently verified before the
next). It owns:

- **Room storage** — `getOrCreateRoom(key, onCreated)`: returns the existing `Room` for a
  key, or builds one via `Room.hpp`'s free `createRoom(key)` and fires `onCreated` exactly
  once (`GameServer::registerRoomBroadcasts` (§5.2), wiring a fresh `Room::game`'s event
  buses to `Outbox`, is the only caller of that hook — `RoomManager` itself has zero
  awareness that `GameEngine` has event buses at all). `rooms()` exposes the underlying map
  directly, since `GameServer::advanceRoom` (the `STATE` broadcast) still needs to reach
  into each `Room`'s contents.
- **Quick-match pairing** — `matchWaiter(rating, eloRange)`/`addWaiter`/`removeWaiter`/
  `reapExpiredWaiters(now)`: a plain `std::vector<WaitingPlayer>`, linear-scanned (handling
  many simultaneous matches at scale is an explicit non-goal for now). `eloRange` is passed
  in per call rather than stored — `RoomManager` owns the waiting-list *mechanics*, not the
  matchmaking *policy* constant (`kEloMatchRangeElo` is a `GameServer.cpp` constant).
- **Reconnection matching** — `reconnect(username, newConnectionId)`: scans every room for
  a `pendingForfeit` whose disconnected seat's username matches, and if found, re-seats that
  `PlayerSession` under the new connection id (clearing the stale selection via
  `Controller::attachGame` as a documented side effect) and clears `pendingForfeit`.
  Recognized by identity (an already-password-authenticated username), not a client-held
  token or room id — quick-match rooms never expose one anyway. Pure state mutation; the
  caller (`GameServer::tryReconnect`) still owns sending `WELCOME`/`ROOM`/`PLAYERS`/
  `RECONNECTED` and logging.
- **Connection routing** — `assignConnectionToRoom`/`isConnectionRouted`/
  `roomForConnection`/`forgetConnectionRoom`: the reverse index (connection id → room key)
  that used to be a bare `main()`-local map. `roomForConnection` returns `Room*` (nullable,
  non-owning) rather than forcing every caller through a second `rooms().at(key)` lookup.
- **Forfeit-grace lifecycle** — `startForfeitCountdown(room, disconnectedColor, graceMs)`
  (called from `GameServer::handleDisconnect`) and `resolveForfeitIfExpired(room, now)`
  (called from `GameServer::advanceRoom`, **one room at a time, inline in the per-room tick
  loop** — deliberately *not* a batch-return like `reapExpiredWaiters`, because this room's
  `game->stop()` must run before this same room's `game->wait(kTickMs)` in the same tick, or
  `GameEngine` would simulate one extra tick of motion after the deadline already passed).
  These two are two halves of one state machine over `Room::pendingForfeit`, and were always
  designed and extracted together, never separately.

### 5.6 `Server/MatchResult.hpp`/`.cpp` — Elo application

One free function, `applyMatchResult(room, winnerColor, accounts, outbox, logger)`: looks up
both players' usernames from `room.players` (must still be present — callers run this
*before* any post-match player/spectator bookkeeping), calls `AccountStore::recordResult`,
and broadcasts the resulting `RATINGS` message. Called from two places —
`GameServer::registerRoomBroadcasts`'s `onGameEnded` bus subscriber (a real king capture) and
`GameServer::advanceRoom`'s `resolveForfeitIfExpired` result (a disconnect timing out) —
which is exactly why it takes every dependency as an explicit parameter rather than
capturing anything: a plain function, not a class, since it has no state of its own to keep
between calls.

### 5.7 `Server/ClickRouter.hpp`/`.cpp` — click validation/routing

One free function, `handleClick(connectionId, decoded, roomManager)` — the last stop for a
`CLICK` message once a connection is fully joined. Does nothing if: the connection isn't
routed to any room, it's a spectator (or a forfeited match's former player), the decoded
message isn't actually a `ClickMessage`, or — the one rule with no local-play equivalent (a
single mouse could always move either color) — it's trying to *select* a piece that isn't
its own color. Once a selection is already active, the second click (the real move/jump
target) passes straight through to `Controller::handleCellClick` without that last check.

### 5.8 `Server/Room.hpp`/`.cpp` and `Server/AccountStore.hpp`/`.cpp` — the data these build on

**`Room.hpp`** (no `.cpp` logic of its own beyond the `createRoom` factory) is the plain-data
record `RoomManager` owns collections of: `PlayerSession` (username, color, that
connection's own `Controller` — both players' `Controller`s wrap the *same* shared
`GameEngine`), `PendingForfeit` (disconnected color + deadline), and `Room` itself (key,
`Board`/`RuleEngine`/`GameEngine`, up to two `players`, any number of read-only
`spectators`, an optional `pendingForfeit`). Connection ids are plain `std::string`
throughout — `Room.hpp`/`RoomManager.hpp` deliberately never `#include` anything under
`Network/`, staying pure Logic-composition so they could, in principle, be reused without a
WebSocket in the picture at all.

**`AccountStore.hpp`/`.cpp`** is the one place allowed to touch `sqlite3_*` directly (same
"one wrapper per external library" convention as `Img`/`SoundPlayer`/`WsServerTransport`):
`login(username, password)` auto-registers a brand-new username (starting rating 1200) or
validates an existing one's password, and `recordResult(winner, loser)` applies a standard
Elo update (K=32). No internal mutex — relies on SQLite's own serialized threading mode
instead.

### 5.9 `Network/` — the transport both processes share

**`WsServerTransport.hpp`/`.cpp`** — the one place allowed to touch `ix::` directly on the
server side; every connection is identified by a small opaque `ConnectionId` string, never
an `ix::WebSocket*`. `onConnect`/`onMessage`/`onDisconnect` fire on IXWebSocket's own
background thread(s) — the thread-safety fact that drives `gameMutex`'s existence in §5.2.
Deliberately untouched by the `GameServer` extraction (§5.2): it still knows nothing about
`RoomManager`, `Room`, or any other domain type, only connection ids and raw text.

**`Protocol.hpp`/`.cpp`** — the wire format: one `std::variant<...>` (`DecodedMessage`)
covering every message either side ever sends (`LoginMessage`, `JoinMessage`,
`ClickMessage`, `WelcomeMessage`, `ForfeitWarningMessage`, `ReconnectedMessage`,
`StateMessage`, and more), plus one `encodeXxx`/`decode` pair per shape. Reuses `Logic`'s
own event structs (`MoveStarted`/`PieceCaptured`/`GameStarted`/`GameEnded`) directly as
payload shapes rather than maintaining a parallel "network event" hierarchy.

**`Logger.hpp`/`.cpp`** — timestamps and prints every `log()` call, appending the same line
to a file; used identically (just a different tag, `"SERVER"` vs `"CLIENT"`) by both
executables so a bad run is debuggable on either side after the fact.

---

## 6. Why it's built this way

**Layering with a strict, one-directional dependency chain** (`model → rules →
movement/collision → realtime → engine → input`, with `io`/`texttests`/`history` depending
only on what they need) means every layer can be understood, tested, and changed in
isolation. You can read `rules/PieceRules.cpp` and understand piece movement completely
without knowing `RealTimeArbiter` exists. You can test `RuleEngine` with nothing but a
`Board` and some pieces — no engine, no UI, no real time at all.

**The `IGameView`/`IInputHandler` boundary being *plain data only*** (never a `Piece*`,
never a `cv::Mat`) is what actually keeps `Logic/` independent of `UI/` in practice, not
just in principle. If `RenderPiece` held a `Piece*` instead of copying out `id`/`type`/
`color`/`x`/`y`, `Core_Interfaces/` would need to `#include` a Logic header, and the
"UI is swappable" claim would be fiction the first time someone tried to actually swap it.

**`RuleEngine` never mutating the board, and `RealTimeArbiter` being the only thing that
actually moves pieces over time**, is what makes it possible to ask "is this move legal
right now" as a pure query, safely, from anywhere (tests, premove re-validation,
`Controller`) — without any risk of a legality check accidentally having a side effect.

**The Strategy pattern in `rules/` (`IPieceRules` + one class per piece type)** avoids a
`switch(piece.type())` sprawling through every place that needs to know how a piece
moves. Adding a new piece type later means adding one new class and one `switch` arm in
`pieceRulesFor` — zero risk to the other five pieces' logic. (See the earlier discussion
in this conversation for the fuller version of this argument, including why the `model/pieces/*`
classes are a separate, weaker case.)

**`GameEngine` as the single front door** — `Controller`, `ScriptRunner`, and tests all
go through it, never touching `RuleEngine`/`RealTimeArbiter` directly — means legality
and real-time behavior can never accidentally diverge between "what the UI does" and
"what a test does." The `ScriptRunner` DSL is deliberately built to prove this: it drives
`Controller`/`GameEngine`/`BoardPrinter`, the exact same path a human clicking a real
window would take.

**Splitting `OpenCvView` into `BoardRenderer` (chess-specific) and `ScoreboardRenderer`
(chess-agnostic)** was a deliberate choice made this session: the side panels don't know
or care what a board, a piece, or a chess move is — they just draw a named, scored,
timestamped list. Keeping that in its own file means it could be reused as-is for a
completely different game, and it keeps `OpenCvView` itself down to orchestration
(what to draw, in what order) rather than a ~230-line mix of three unrelated concerns.

---

## 7. Suggested reading order if you want to actually absorb this

1. `Logic/include/model/Enums.hpp` + `GameConfig.hpp` — the vocabulary everything else
   uses.
2. `Logic/include/model/pieces/Piece.hpp` + `Board.hpp`/`.cpp` — what "the board" actually
   is.
3. `Logic/include/rules/PieceRules.hpp`/`.cpp` — how legality is computed (no real time
   involved yet — the easiest layer to reason about fully).
4. `Logic/include/engine/GameEngine.hpp`/`.cpp` — read `requestMove` and `wait` closely;
   this is the layer everything above it calls.
5. `Logic/include/realtime/RealTimeArbiter.hpp`/`.cpp` — the hardest file, but by this
   point you'll have every concept (`PieceState`, `MoveValidation`, `GameRecord`) it
   assumes.
6. `Logic/include/input/Controller.hpp`/`.cpp` — short, and ties everything above back to
   "a click."
7. `main.cpp` (§4.17) — the client's composition root: `establishConnection` (login/room/
   play) then `runGameSession` (the actual render loop).
8. `UI/Windows/LoginScreen.hpp` + `RoomChoiceScreen.hpp` + `PlayConfirmScreen.hpp` (§4.15)
   — the three pre-game screens `establishConnection` drives, each Network-agnostic.
9. `Client/RemoteGameProxy.hpp`/`.cpp` (§4.16) — the client-side stand-in for a local
   `GameEngine`, and its split between plain-mutex state and queued bus events.
10. `UI/OpenCV/OpenCvView.cpp` + `BoardRenderer.cpp` — how a `RenderPiece` list actually
    becomes pixels.
11. `Server/GameServer.hpp`/`.cpp` (§5.2) + `RoomManager.hpp`/`.cpp` (§5.5) — the
    authoritative backend every client above is actually talking to: one real `GameEngine`
    per room, reached from `Server/ClickRouter.cpp` rather than a local `Controller` call.

Section 3 of this document (the traced click) is the fastest way to re-orient yourself in
the core Logic mechanics any time you get lost in a specific file — but it predates the
network split and stops at `Controller`/`GameEngine`: in the real client-server build, a
click travels `OpenCvView::onMouse` → `BoardClickHandler` → `RemoteGameProxy::sendClick`
→ (the network) → `Server/ClickRouter::handleClick` → `Controller::handleCellClick`, not
straight from `main.cpp` to `Controller` in one process. Section 3's walk is still
accurate for everything *after* that hand-off (`Controller` onward) — just read it as the
server-side continuation, not the whole path.
