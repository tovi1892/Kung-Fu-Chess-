# Kung Fu Chess — Architecture Diagrams

A companion to `docs/PROJECT_GUIDE.md`: that document is reference-first (read a section to
look something up); this one is visual-first (look at a diagram, then read the walkthrough
underneath it to actually follow the logic). Every diagram here was drawn by reading the
current source directly — method names, call order, and branch conditions are all
cross-checked against the real code, not reconstructed from memory.

**Status**: Part 1 (the big picture) and Part 2 (four server-flow diagrams) are below.
Part 3 (every class/function under `Server/`, `Network/`, `Client/`, in depth) follows once
you've confirmed these read correctly — no point writing a long prose section under a diagram
that needs fixing first.

---

## Part 1 — The big picture

```mermaid
flowchart TD
    subgraph EXE["Two separate executables"]
        direction LR
        App(["kungfu_app<br/>(main.cpp, repo root)"])
        Srv(["kungfu_server<br/>(Server/main.cpp)"])
    end

    App --> UI
    App --> Client
    App --> Network
    App --> Logic
    App --> CI

    Srv --> Server

    Server --> Logic
    Server --> Network

    Client --> Network
    Client --> Logic
    Client --> CI

    UI --> Logic
    UI --> CI

    Network --> Logic
    Network --> CI

    Logic["Logic/<br/>(chess rules + real-time engine)"]
    CI["Core_Interfaces/<br/>(IGameView, IInputHandler,<br/>RenderPiece, Banner, ...)"]
    UI["UI/<br/>(OpenCV rendering,<br/>Win32 login/room/play screens)"]
    Network["Network/<br/>(WebSocket transport +<br/>wire protocol)"]
    Client["Client/<br/>(RemoteGameProxy)"]
    Server["Server/<br/>(GameServer, RoomManager,<br/>ConnectionSessions, ...)"]
```

**Reading the arrows**: an arrow from A to B means "A `#include`s B's headers" (verified
against the actual `#include` lines in every file, not assumed from folder names). Two nodes
with no arrow between them genuinely never reference each other's types.

### What each module owns, and why the arrows point this way

- **`Logic/`** is the actual chess-with-real-time engine (`GameEngine`, `RuleEngine`,
  `RealTimeArbiter`, `Controller`, `Board`, the piece rule classes). It depends on **nothing**
  outside itself except two `.cpp` files pulling in `Core_Interfaces/IGameView.hpp` purely to
  get the concrete `RenderPiece` struct definition for `getRenderState()`. It has zero
  awareness that OpenCV, Win32, WebSockets, or a network exists. This is deliberate: it's
  what lets `Logic/` be unit-tested completely on its own (`kungfu_tests`), and it's what
  makes both a local single-process game *and* a networked one possible from the same
  engine — `Server/` runs a real `GameEngine` per match; nothing in `Logic/` had to change to
  make that work.
- **`Core_Interfaces/`** is two abstract types (`IGameView`, `IInputHandler`) plus a handful
  of plain-data structs (`RenderPiece`, `BoardHighlight`, `Scoreboard`, `Banner`). It's the
  seam that lets a renderer be swapped without touching `Logic/`: neither side ever hands the
  other a real `Piece*` or `cv::Mat`, only plain data.
- **`UI/`** is the OpenCV renderer (`OpenCvView`, `BoardRenderer`, `ScoreboardRenderer`,
  `AssetManager`) plus the Win32 login/room/play screens. It depends on `Logic/` only for a
  few enums (`PieceType`/`PlayerColor` for choosing sprites, `GameConfig::kBoardSize` for
  layout) — never on `GameEngine`, `Controller`, or any live game object. It has **zero**
  dependency on `Network/` or `Client/`: `UI/Windows/`'s three pre-game screens
  (`LoginScreen`, `RoomChoiceScreen`, `PlayConfirmScreen`) return plain data (a username/
  password pair, a `RoomChoice`) and the composition root (`main.cpp`) is the only place that
  turns that into an actual network call.
- **`Network/`** is the wire format (`Protocol.hpp`/`.cpp`) and the two WebSocket transport
  wrappers (`WsServerTransport`, `WsClientTransport`). It depends on `Logic/` because the
  protocol deliberately *reuses* `Logic`'s own event structs (`MoveStarted`, `PieceCaptured`,
  `GameStarted`, `GameEnded`) and a few model types (`PlayerColor`, `PieceType`, `Position`)
  directly as wire-message payloads, rather than maintaining a parallel "network event"
  hierarchy that could drift out of sync with the real one.
- **`Client/`** (just `RemoteGameProxy`) is a client-side stand-in for a local `GameEngine` —
  it depends on `Network/` (to actually talk to the server) and `Logic/` (to expose the same
  event-bus shape a real `GameEngine` would). It has **zero** dependency on `UI/`: nothing
  about `RemoteGameProxy` assumes OpenCV or Win32 exists, which is what makes it plausible
  that a completely different renderer could reuse it unchanged.
- **`Server/`** depends on `Logic/` directly (it owns one real `Board`/`RuleEngine`/
  `GameEngine`/`Controller` per room — see `Server/Room.hpp`) and on `Network/` (to talk to
  clients). It has **zero** dependency on `Core_Interfaces/` or `UI/` — the server never
  renders anything, so it has no use for `RenderPiece`/`IGameView` at all; it only ever
  produces the *text* form of game state (`STATE ...`) via `Network/Protocol.cpp`'s
  `encodeState`.
- The two **executables** are where everything converges: `main.cpp` (client) is the only
  file that knows about `UI/`, `Client/`, `Network/`, and `Logic/` all at once (see
  `PROJECT_GUIDE.md` §4.17); `Server/main.cpp` is deliberately much thinner — it only
  constructs objects and calls `GameServer::run()` (diagram 4 below shows why: everything
  that would otherwise clutter `main()` is `GameServer`'s own internal call structure).

The one rule that makes all of this hold together: **`Logic/` never depends on anything
above it.** Every other module can change independently (swap the renderer, add a new wire
message, rewrite the server's connection handling) without `Logic/` ever needing to know.

---

## Part 2 — Server-side flow, in detail

### 1. New player connects, logs in, quick-matches, game starts

```mermaid
sequenceDiagram
    actor A as Client A (new user)
    actor B as Client B (existing user)
    participant WST as WsServerTransport
    participant GS as GameServer
    participant CS as ConnectionSessions
    participant AS as AccountStore
    participant RM as RoomManager
    participant OB as Outbox

    A->>WST: opens WebSocket connection
    WST->>GS: handleConnect(idA)
    GS->>CS: markPending(idA)

    A->>WST: "LOGIN alice pw123"
    WST->>GS: handleMessage(idA, text)
    GS->>GS: decode(text) -> LoginMessage
    GS->>CS: isPending(idA)? yes
    GS->>GS: handleLogin(idA, decoded)
    GS->>AS: login("alice", "pw123")
    AS->>AS: username not found in DB
    AS->>AS: generate random salt, PBKDF2-hash password,<br/>INSERT new row (rating = 1200)
    AS-->>GS: {success:true, accountCreated:true, rating:1200}
    GS->>CS: authenticate(idA, "alice", 1200)
    GS->>OB: enqueue(idA, "LOGIN_OK 1200 1")
    GS->>OB: flush()
    OB->>WST: send(idA, "LOGIN_OK 1200 1")
    WST->>A: "LOGIN_OK 1200 1"

    A->>WST: "JOIN Q"
    WST->>GS: handleMessage(idA, text)
    GS->>CS: isPending(idA)? no
    GS->>CS: find(idA) -> {alice, 1200}
    GS->>RM: isConnectionRouted(idA)? no
    GS->>GS: handleJoin(idA, decoded, "alice", 1200)
    GS->>RM: tryReconnect: reconnect("alice", idA)
    RM-->>GS: nullopt (no pending forfeit anywhere)
    GS->>RM: handleQuickMatch: matchWaiter(1200, eloRange)
    RM-->>GS: nullopt (nobody else waiting yet)
    GS->>RM: addWaiter({idA, "alice", 1200, deadline = now+60s})
    GS->>OB: flush() (nothing was queued)

    Note over B: B already has an account - same LOGIN round trip,<br/>but AccountStore finds the row and checks the<br/>PBKDF2 hash instead of inserting a new one

    B->>WST: "LOGIN bob pw456"
    WST->>GS: handleMessage(idB, text)
    GS->>AS: login("bob", "pw456")
    AS->>AS: username found - derive PBKDF2 hash from<br/>submitted password + stored salt, compare to<br/>stored hash
    AS-->>GS: {success:true, accountCreated:false, rating:1180}
    GS->>CS: authenticate(idB, "bob", 1180)
    GS->>OB: enqueue + flush "LOGIN_OK 1180 0"

    B->>WST: "JOIN Q"
    WST->>GS: handleMessage(idB, text)
    GS->>GS: handleJoin(idB, decoded, "bob", 1180)
    GS->>RM: tryReconnect: reconnect("bob", idB) --> nullopt
    GS->>RM: handleQuickMatch: matchWaiter(1180, eloRange)
    RM-->>GS: {idA, "alice", 1200} (found within range, removed from waiting list)
    GS->>RM: allocateQuickMatchRoomKey() -> "quickmatch-0"
    GS->>RM: getOrCreateRoom("quickmatch-0", registerRoomBroadcasts)
    RM->>RM: createRoom(): fresh Board + RuleEngine + GameEngine
    RM->>GS: onCreated(room) fires once
    GS->>GS: registerRoomBroadcasts(room):<br/>subscribe to the 4 GameEngine event buses
    GS->>RM: assignConnectionToRoom(idA, key), assignConnectionToRoom(idB, key)
    GS->>GS: room.players[idA] = {White, new Controller}<br/>room.players[idB] = {Black, new Controller}
    GS->>OB: enqueue WELCOME(White) to idA, WELCOME(Black) to idB
    GS->>OB: enqueueToRoom PLAYERS("alice","bob")
    GS->>GS: room.game->start()
    GS->>OB: flush()
    OB->>WST: send to both connections
    WST->>A: "WELCOME w" / "PLAYERS alice bob"
    WST->>B: "WELCOME b" / "PLAYERS alice bob"

    loop every 16ms (GameServer::run()'s tick loop)
        GS->>GS: tick() -> advanceRoom(room, now)
        GS->>GS: room.game->wait(16) - advances RealTimeArbiter
        GS->>OB: enqueue STATE to idA and idB
        GS->>OB: flush()
        OB->>WST: send STATE
        WST->>A: "STATE ..."
        WST->>B: "STATE ..."
    end
```

**Why LOGIN and JOIN are two separate messages, not one.** Authentication ("who are you") and
routing ("what match do you want") are genuinely independent decisions, and the client's own
UI mirrors that split: `LoginScreen` resolves first, then `RoomChoiceScreen`/
`PlayConfirmScreen` decide the room — a player might retry `LoginScreen` after a wrong
password without ever having chosen a room yet. Keeping them as two messages is what lets
`ConnectionSessions` (identity only) and `RoomManager` (rooms/matchmaking only) stay
completely independent of each other — `ConnectionSessions` never needs to know a room exists,
and `RoomManager` never needs to know how a username was authenticated.

**Why `tryReconnect` runs before quick-match/room-join even on this first-ever JOIN.** It costs
nothing when there's genuinely nothing to reconnect to (`RoomManager::reconnect` just scans
every room for a matching pending forfeit and returns immediately if none exists), and it means
`GameServer` never has to ask "is this actually a reconnection?" — the reconnecting player's
client goes through the exact same LOGIN → JOIN path as a brand-new player, clicking whatever
they want on `RoomChoiceScreen`; the server alone decides whether that's actually a resume.

**Why the tick loop runs even before two players exist.** `GameEngine::wait(ms)` is a no-op
until `start()` has been called, so ticking a room that's still waiting for a second player
costs nothing — there's no special "is this room ready to tick" check needed anywhere.

**Why `Outbox` exists at all, instead of just calling `WsServerTransport::send` directly.**
`ix::WebSocket::send()` can, if it detects a dead connection, synchronously invoke that same
transport's Close callback on the calling thread before `send()` returns — and that Close
callback is `GameServer::handleDisconnect`, which tries to lock `gameMutex`. If `send()` were
called while `gameMutex` was already held (which every branch above is), that's a
self-deadlock. `Outbox` splits every send into "queue while locked" then "flush after
unlocking," making that interleaving impossible by construction.

**Why `gameMutex` is needed in the first place.** `WsServerTransport` fires `onConnect`/
`onMessage`/`onDisconnect` on IXWebSocket's own background I/O thread(s) (see its own
`clients_`/`clientsMutex_`, which is a *separate* mutex protecting only its connection-id → 
`ix::WebSocket*` map). `GameEngine`/`Controller`, on the other hand, were built assuming
single-threaded access. `gameMutex` is the one lock that makes it safe for two connections'
messages (or a message and a disconnect) to arrive concurrently on different IXWebSocket
threads and still touch `RoomManager`/`ConnectionSessions`/the domain state safely.

---

### 2. Disconnect mid-game → forfeit countdown → reconnect *or* forfeit resolves

```mermaid
sequenceDiagram
    actor A as Client A (White, alice)
    participant WST as WsServerTransport
    participant GS as GameServer
    participant RM as RoomManager
    participant OB as Outbox
    actor B as Client B (Black, bob)

    Note over A,B: A game is already Running - alice = White, bob = Black

    A--xWST: connection drops (network loss, app closed, ...)
    WST->>GS: handleDisconnect(idA)
    GS->>RM: removeWaiter(idA) (no-op - not in the quick-match queue)
    GS->>RM: roomForConnection(idA) -> the room
    GS->>GS: room.players.find(idA) found, room.game->isRunning() true
    GS->>RM: startForfeitCountdown(room, White, 20000ms)
    RM->>RM: room.pendingForfeit = {White, now + 20s}
    GS->>OB: enqueueToRoom FORFEIT_WARNING(White, 20000)
    GS->>RM: forgetConnectionRoom(idA)
    GS->>GS: sessions.forget(idA)
    GS->>OB: flush()
    OB->>WST: send to bob's connection
    WST->>B: "FORFEIT_WARNING w 20000"
    Note over B: client shows "Opponent disconnected - auto-win in 20s"<br/>and counts down locally (RemoteGameProxy's own clock estimate)

    alt (a) alice reconnects within the 20s window
        A->>WST: opens a NEW WebSocket connection (idA2)
        WST->>GS: handleConnect(idA2) -> sessions.markPending(idA2)
        A->>WST: "LOGIN alice pw123" (same credentials)
        WST->>GS: handleMessage -> handleLogin -> AccountStore confirms password
        GS->>GS: sessions.authenticate(idA2, "alice", rating); LOGIN_OK
        A->>WST: "JOIN Q" (or Create/Join - doesn't matter which)
        WST->>GS: handleMessage(idA2, text) -> handleJoin(idA2, decoded, "alice", rating)
        GS->>RM: tryReconnect: reconnect("alice", idA2)
        RM->>RM: scan rooms for pendingForfeit whose<br/>disconnected seat's username == "alice" - found
        RM->>RM: move PlayerSession from old key to idA2,<br/>controller->attachGame(...) clears stale selection,<br/>pendingForfeit.reset()
        RM-->>GS: {room, White}
        GS->>RM: assignConnectionToRoom(idA2, room.key)
        GS->>OB: enqueue WELCOME(White), ROOM(key), PLAYERS(...) to idA2
        GS->>OB: enqueueToRoom RECONNECTED(White)
        GS->>OB: flush()
        OB->>WST: send to both
        WST->>A: "WELCOME w" / "ROOM ..." / "PLAYERS ..."
        WST->>B: "RECONNECTED w"
        Note over B: client shows "Opponent reconnected!" and the<br/>countdown display disappears
        Note over A,B: tick loop continues normally - pendingForfeit is<br/>gone, resolveForfeitIfExpired() is a no-op for this room
    else (b) nobody reconnects before the deadline
        loop every 16ms, for ~20 seconds
            GS->>GS: tick() -> advanceRoom(room, now)
            GS->>RM: resolveForfeitIfExpired(room, now)
            RM->>RM: now < pendingForfeit.deadline - not yet, returns nullopt
        end
        GS->>GS: tick() -> advanceRoom(room, now) - the tick where now >= deadline
        GS->>RM: resolveForfeitIfExpired(room, now)
        RM->>RM: winner = Black (the color that DIDN'T disconnect)
        RM->>RM: move remaining players to spectators,<br/>players.clear(), game->stop(), pendingForfeit.reset()
        RM-->>GS: {winner: Black}
        GS->>OB: enqueueToRoom FORFEIT(Black)
        GS->>GS: applyMatchResult(room, Black, accounts, outbox, logger)
        GS->>OB: enqueueToRoom RATINGS(newWhiteRating, newBlackRating)
        GS->>OB: flush()
        OB->>WST: send to bob (now a spectator of his own former match)
        WST->>B: "FORFEIT b" / "RATINGS ..."
    end
```

**Why the disconnected player's seat isn't dropped immediately.** Their username is still
needed for the eventual Elo update — `applyMatchResult` reads `room.players` to find both
usernames, so the seat has to survive until the forfeit (or reconnection) actually resolves,
not just until the socket closes.

**Why reconnection is matched by *username*, not a session token or the room id.** Phase 4
already added password-authenticated accounts, so a successful `LOGIN` is already stronger
proof of identity than any client-held token could be (a token can be lost or spoofed; a
password can't be guessed in one try). It also sidesteps a real problem: quick-match rooms
never expose a room id to the client at all (there's nothing user-facing to type back in), so
matching by room id wouldn't even be possible for a quick-matched player who disconnects.

**Why `resolveForfeitIfExpired` is checked once per room, per tick, inline — not batched.**
`room.game->stop()` (part of resolving the forfeit) has to run *before* that same room's
`game->wait(kTickMs)` call in the *same* tick, or `GameEngine` would simulate one extra 16ms
tick of movement after the deadline had already technically passed. Batching this into a
separate up-front pass (the way quick-match's `reapExpiredWaiters` works) would still happen
to be safe here too (different rooms never interact), but keeping it inline, at the exact spot
the original code had it, makes the ordering *provably* unchanged rather than "equivalent by
argument" — this was a deliberate, carefully-checked decision (see the session history behind
this refactor).

---

### 3. A room's lifecycle, as a state diagram

Your proposed framing (`waiting → matched → playing → disconnected/grace-period →
reconnected-or-forfeited → game ended`) is close, but the real code draws a few lines in
different places than that suggests — corrected below, with the two most important
corrections called out right after the diagram.

```mermaid
stateDiagram-v2
    [*] --> Empty : getOrCreateRoom() called for a brand-new key
    Empty --> WaitingForOpponent : one player seated (Create/Join, room.players.size()==1)
    Empty --> Playing : quick-match seats BOTH players in the same call - never sits at "waiting" with one
    WaitingForOpponent --> Playing : second player seated -> room.game->start()

    Playing --> SeatDisconnectedGrace : a seated player's connection drops mid-game (room.game->isRunning())
    SeatDisconnectedGrace --> Playing : reconnect() matches this username within the grace window
    SeatDisconnectedGrace --> Forfeited : resolveForfeitIfExpired() - grace window elapsed with no reconnect

    Playing --> GameEnded : a king is captured (GameEngine transitions itself to Finished)

    Forfeited --> [*] : room object stays alive forever in RoomManager - rooms are never removed (explicit non-goal)
    GameEnded --> [*] : same - stays alive forever
```

**Correction 1 — a "forfeited" game and a "king captured" game are NOT the same underlying
state.** A real king capture sets `GameEngine`'s internal state to `Finished`
(`GameEngine::isFinished()` becomes true). A resolved forfeit calls `room.game->stop()`, which
sets `GameEngine`'s internal state to **`Paused`**, not `Finished`. Both make
`GameEngine::isRunning()` false (so the tick loop's `game->wait()` becomes a no-op either way,
which is why nothing outwardly *looks* different), but if you ever go looking for
`isFinished()` after a forfeit, it will say `false` — a genuinely easy trap if you assume "the
match is over" is one single state everywhere in the code.

**Correction 2 — "matched" isn't always a separate state from "waiting."** Your framing
implies every room passes through a `waiting` state before becoming `playing`. That's true for
`CreateRoom`/`JoinRoom` (one player sits in `WaitingForOpponent` until a second joins), but
**not** for quick-match: `RoomManager::matchWaiter` only ever creates a room at the exact
moment a second waiting player is found, so a quick-matched room is born already in `Playing`
— it never has a "waiting" phase of its own (the *player* waits, in `RoomManager`'s
`quickMatchWaiting_` list, before any room exists at all).

**One more thing this diagram deliberately leaves out**: a **spectator** (the 3rd+ connection
to `JOIN` an already-full room) isn't a room state at all — it's a per-connection role that
can start the instant `Playing` begins and lasts until that connection disconnects, completely
orthogonal to whether the room itself is `Playing`, `SeatDisconnectedGrace`, `Forfeited`, or
`GameEnded`. Spectators keep receiving `STATE` broadcasts throughout, including after a
forfeit resolves (the forfeited seats' former players are moved into the spectator set too, so
they can keep watching).

---

### 4. `GameServer`'s internal call structure

```mermaid
flowchart TD
    HM["handleMessage(id, text)"] --> DEC["decode(text)"]
    DEC --> Q1{"sessions.isPending(id)?"}

    Q1 -- yes --> HL["handleLogin(id, decoded)"]
    HL --> HL1{"is it a LoginMessage?"}
    HL1 -- no --> HLret["return - ignored"]
    HL1 -- yes --> HL2["accounts.login(username, password)"]
    HL2 --> HL3{"result.success?"}
    HL3 -- yes --> HL4["sessions.authenticate(...)<br/>outbox: LOGIN_OK"]
    HL3 -- no --> HL5["outbox: LOGIN_FAIL"]

    Q1 -- no --> Q2{"sessions.find(id) has a value<br/>AND NOT roomManager.isConnectionRouted(id)?"}

    Q2 -- yes --> HJ["handleJoin(id, decoded, username, rating)"]
    HJ --> HJ1{"is it a JoinMessage?"}
    HJ1 -- no --> HJret["return - ignored"]
    HJ1 -- yes --> TR["tryReconnect(id, username)"]
    TR --> TR1{"roomManager.reconnect() found<br/>a matching pending forfeit?"}
    TR1 -- yes --> TRdone["re-seat under this connection id;<br/>WELCOME + ROOM + PLAYERS + RECONNECTED<br/>-- done, whatever join.mode was is ignored"]
    TR1 -- no --> HJ2{"join.mode?"}

    HJ2 -- QuickMatch --> HQM["handleQuickMatch(id, username, rating)"]
    HQM --> HQM1{"matchWaiter() found a<br/>waiting opponent in range?"}
    HQM1 -- yes --> HQM2["create room, seat both<br/>White/Black, WELCOME + PLAYERS,<br/>game.start()"]
    HQM1 -- no --> HQM3["addWaiter() - this connection<br/>now waits its turn"]

    HJ2 -- "CreateRoom or JoinRoom" --> HRJ["handleRoomJoin(id, join, username)"]
    HRJ --> HRJ1{"room.players.size() < 2?"}
    HRJ1 -- yes --> HRJ2["seat as player (White if empty,<br/>else Black); if now 2 players,<br/>PLAYERS + game.start()"]
    HRJ1 -- no --> HRJ3["seat as spectator:<br/>SPECTATE + ROOM + PLAYERS"]

    Q2 -- no --> CR["ClickRouter::handleClick(id, decoded, roomManager)<br/>— a domain function, not a GameServer method;<br/>a no-op if this connection isn't actually routed"]
```

**Why this is broken into ten small methods instead of one long `onMessage` handler.** Every
branch point above corresponds to a genuinely different question ("who is this," "what do they
want," "which of three ways do they want it") — collapsing them back into one function would
just mean re-introducing the exact wall of nested `if`s this shape was extracted specifically
to get away from. Each named method reads as an answer to one question, and none of them need
to know how the others reached their own decision.

**Why `tryReconnect`/`handleQuickMatch`/`handleRoomJoin` are mutually exclusive, checked in
that exact order.** Reconnection is checked first specifically so it wins over whatever the
client actually clicked (see the "why matched by username" note above) — a reconnecting
player's client has no special "resume" button, it just goes through the ordinary
`RoomChoiceScreen` flow again. Quick-match and Create/Join are mutually exclusive by
construction (`join.mode` is one enum value), so there's no real ordering question between
those two — only reconnection needs to jump the queue.

**Why `ClickRouter::handleClick` isn't a `GameServer` method at all**, even though it's called
from the same `handleMessage` dispatch as everything else. It doesn't need `GameServer`'s own
state (`accounts_`/`sessions_`/`server_`/etc.) — only `RoomManager`, which it already receives
as a parameter. Keeping it a free function in `Server/ClickRouter.cpp` (domain layer, §5.7 of
`PROJECT_GUIDE.md`) rather than a `GameServer` method is exactly the Ports & Adapters line this
whole class exists to draw: `GameServer` decides *which* domain call to make; the domain
classes themselves decide what happens once called.

---

Let me know if any of these four diagrams (or Part 1's big picture) don't match your mental
model before I write Part 3 — the full class-by-class breakdown of `Server/`, `Network/`, and
`Client/`.
