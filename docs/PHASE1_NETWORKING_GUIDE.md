# Phase 1 Networking — Full Guide

Companion to `docs/PROJECT_GUIDE.md` (which covers the original single-process app).
This document covers everything added or changed to turn that local, one-mouse app into
a real client/server game over the network — file by file, function by function, with
the reasoning behind each piece and how it sets up the later phases (usernames, rooms,
accounts, and eventually a genuinely internet-facing server).

---

## 1. The big picture, first

Before Phase 1, one executable (`kungfu_app.exe`) did everything: it built the `Board`,
`GameEngine`, and `Controller` itself, and one mouse could move either color.

Phase 1 splits that one process into **two kinds of process**:

- **`kungfu_server.exe`** (brand new) — owns the *real* game. It builds the `Board`,
  `RuleEngine`, `GameEngine`, exactly like the old `main.cpp` used to. It is the only
  process that ever calls `GameEngine::requestMove`. It has no window, no OpenCV, no
  pixels — its whole job is deciding what's legal and telling everyone what happened.
- **`kungfu_app.exe`** (rewritten) — now just a *window that talks to a server*. It owns
  no game logic at all anymore. It sends `"the user clicked here"` messages out, and
  receives `"here's what the board looks like now"` / `"a move just happened"` messages
  back, and draws whatever it's told.

The two talk over a **WebSocket connection** (a two-way, always-open network
connection — think of it as a phone call that stays connected, versus a letter you send
and wait for a reply to), using **plain text messages** that this project defines itself
(not a pre-made protocol from a library).

This split is the foundation every later phase builds on: usernames, rooms, accounts,
and Elo don't change this shape — they just add more information to the messages and
more logic inside the server. The client stays "dumb" (just a window) the whole way
through.

---

## 2. The build system: `CMakeLists.txt`

Before touching any game code, the *build* had to be restructured, because now there are
two different programs (`kungfu_server.exe`, `kungfu_app.exe`) that need *different*
pieces of the codebase — the server must never need OpenCV, for instance.

### What existed before

One static library, `kungfu_engine`, bundled *every* `.cpp` file under both `Logic/` and
`UI/` together, and both the app and the tests linked that one blob.

### What changed, and why each piece exists now

```cmake
# --- kungfu_logic: the engine, no OpenCV, no networking --------------------
file(GLOB_RECURSE LOGIC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Logic/src/*.cpp")
add_library(kungfu_logic STATIC ${LOGIC_SOURCES})
target_include_directories(kungfu_logic PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Logic/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Core_Interfaces
)
```
`kungfu_logic` is just the old `Logic/src/*.cpp` files (nothing from `UI/`). This is the
library that will eventually be shared by *every* future piece that needs real chess
rules and nothing else — today that's `kungfu_server` and the test executables.

```cmake
# --- kungfu_network: WebSocket transport + wire protocol --------------------
file(GLOB_RECURSE NETWORK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Network/*.cpp")
add_library(kungfu_network STATIC ${NETWORK_SOURCES})
target_include_directories(kungfu_network PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(kungfu_network PUBLIC kungfu_logic ixwebsocket ws2_32)
```
A brand-new library for the brand-new `Network/` folder (section 3 below). Two things
worth understanding:
- `target_include_directories(... PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})` adds the **project
  root** (not `Network/` itself) to the include path. That's *why* every file elsewhere
  writes `#include "Network/Protocol.hpp"` — the full path from the project root, the
  exact same convention `Logic/include` already uses (`#include "model/Position.hpp"`).
- It links `ixwebsocket` (the third-party library, explained in section 3) and `ws2_32`
  (Windows' own low-level networking library — sockets don't work on Windows without it).

```cmake
# --- kungfu_ui: the OpenCV-backed view, depends on kungfu_logic + kungfu_network ----
file(GLOB_RECURSE UI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/UI/*.cpp")
add_library(kungfu_ui STATIC ${UI_SOURCES})
target_include_directories(kungfu_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/UI)
target_link_libraries(kungfu_ui PUBLIC kungfu_logic kungfu_network)
```
The old `UI/*.cpp` files, now their own library. It links `kungfu_network` — this is a
detail worth remembering: `UI/NetClient/RemoteGameProxy.cpp` (section 5) physically lives
inside `UI/`, so it gets swept into this library by the `GLOB_RECURSE`, and it needs
`Network/WsClientTransport.hpp` to compile. That's the *only* reason `kungfu_ui` needs to
know about networking at all — conceptually, "the renderer" and "the network client"
are still two separate concerns; they just happen to be packaged in the same `.lib` file
here for simplicity.

```cmake
# --- Executables -------------------------------------------------------------
add_executable(kungfu_server Server/main.cpp)
target_link_libraries(kungfu_server PRIVATE kungfu_logic kungfu_network)

add_executable(kungfu_app main.cpp)
target_link_libraries(kungfu_app PRIVATE kungfu_ui)
```
This is the payoff of the whole split: `kungfu_server` links `kungfu_logic` +
`kungfu_network` — **never `kungfu_ui`, never OpenCV**. That's not just tidiness — it's
why `kungfu_server.exe` doesn't need OpenCV's DLL on its `PATH` to even start, unlike the
client. (`kungfu_app` only needs to list `kungfu_ui` — it gets `kungfu_network`
automatically, since `kungfu_ui` already links it `PUBLIC`.)

### The new dependency: IXWebSocket

```cmake
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.5
)
FetchContent_MakeAvailable(ixwebsocket)
```
Fetched the exact same way Catch2 already was — `FetchContent` downloads the source and
builds it as part of this project, no manual install step. `USE_TLS OFF` / `USE_ZLIB
OFF` turn off two optional features we don't need yet: TLS is `wss://` (encrypted
WebSocket, the `https` of WebSockets) and would need OpenSSL as a *further* dependency;
`ZLIB` is optional message compression. Phase 1 only ever talks plain `ws://` on a LAN or
`localhost`, so neither is worth the extra dependency weight right now — this is exactly
the kind of thing that *would* need turning on for a real internet deployment (more on
that in section 7).

---

## 3. `Network/` — the transport and the wire protocol

This is the new top-level module — a sibling to `Logic/`, `UI/`, `Core_Interfaces/`. It
has one job: turn "a message" into text that can cross a WebSocket, and back. It knows
nothing about *rendering* and nothing about *deciding whether a move is legal* — both of
those stay exactly where they already were (`Logic/` for legality, `UI/` for rendering).

### `Network/Protocol.hpp` / `Protocol.cpp` — what a message actually looks like

The single most important design choice here: **the message payloads reuse Logic's own
event structs directly.**

```cpp
using DecodedMessage = std::variant<std::monostate, ClickMessage, WelcomeMessage, StateMessage,
                                     MoveStarted, PieceCaptured, GameStarted, GameEnded>;
```

`MoveStarted`, `PieceCaptured`, `GameStarted`, `GameEnded` are the exact same structs
`Logic/events/GameEvents.hpp` already defined for the local event bus (built the session
before this one). There's no separate "network version" of a move that has to be kept in
sync with the "real" one — one shape, two uses. `ClickMessage`, `WelcomeMessage`, and
`StateMessage` are new — they don't correspond to anything in `Logic/`, because they're
purely about the network conversation itself (a click, a welcome greeting, a snapshot of
positions), not about chess.

`std::variant` is C++'s built-in "this is exactly one of these N types, and I'll tell you
which" — `decode()` returns one, and callers use `std::get_if<T>(&decoded)` to ask "is it
a `T`?" and get a pointer to it if so, or `nullptr` if not. `std::monostate` is the
"none of the above / malformed" case — every decode function has to handle that
possibility, so it's baked into the type itself rather than being a separate error path
someone could forget to check.

**Every message is one line of text, except `STATE`, which is several lines glued
together with `\n`.** WebSocket already delivers whole messages as one unit (unlike raw
TCP, which is just a stream of bytes with no message boundaries at all) — so a multi-line
`STATE` snapshot can be sent as a single WebSocket frame with embedded newlines, and the
receiver knows exactly where it starts and ends without any custom length-prefixing.
This is one of the concrete reasons a WebSocket library earns its keep here instead of
raw sockets.

**Piece encoding reuses the exact convention `BoardParser`/`BoardPrinter` already
use** — a two-character token like `"wR"` (white rook) or `"bP"` (black pawn):

```cpp
char colorChar(PlayerColor color) { return color == PlayerColor::White ? 'w' : 'b'; }
char typeChar(PieceType type) {
    switch (type) {
        case PieceType::King:   return 'K';
        case PieceType::Queen:  return 'Q';
        case PieceType::Rook:   return 'R';
        case PieceType::Bishop: return 'B';
        case PieceType::Knight: return 'N';
        case PieceType::Pawn:   return 'P';
    }
    return 'P';
}
std::string pieceToken(PlayerColor color, PieceType type) {
    return std::string(1, colorChar(color)) + std::string(1, typeChar(type));
}
```
Nothing new conceptually here — it's the same `K`/`Q`/`R`/`B`/`N`/`P` mapping
`BoardPrinter::pieceToken` already uses for the local text-DSL, just reimplemented for
raw `PlayerColor`/`PieceType` values instead of a `Piece*` (since here there's no actual
`Piece` object — just the type/color the server already extracted from one).

**Encoding functions** — one per outgoing message shape, each just a `std::ostringstream`
built up field by field:

```cpp
std::string encodeClick(int row, int col) {
    return "CLICK " + std::to_string(row) + " " + std::to_string(col);
}
```
The simplest one: `"CLICK 3 4"`. This is the *only* message the client ever sends —
Phase 1's whole client→server vocabulary is one line.

```cpp
std::string encodeState(const std::vector<RenderPiece>& pieces) {
    std::ostringstream out;
    out << "STATE\n" << std::fixed;
    out.precision(6);
    for (const auto& rp : pieces) {
        out << rp.id << " " << pieceToken(static_cast<PlayerColor>(rp.color), static_cast<PieceType>(rp.type))
            << " " << rp.x << " " << rp.y << " " << rp.state << " " << rp.cooldownMs << " "
            << rp.cooldownTotalMs << "\n";
    }
    out << "END";
    return out.str();
}
```
Builds one `STATE` message: the literal word `STATE`, then one line per piece with every
field `RenderPiece` already carries (`id`, a 2-char token for type+color, `x`/`y`
position, `state`, and the two cooldown fields), then the literal word `END` as a
terminator. `std::fixed` + `precision(6)` matters specifically for `x`/`y` — those are
fractional (a piece mid-move sits at e.g. `x=1.43`), and without forcing fixed-point
notation, C++'s default number formatting can switch to scientific notation for some
values, which the decoder isn't expecting.

```cpp
std::string encodeMoveStarted(const MoveStarted& event) {
    std::ostringstream out;
    out << "MOVE " << pieceToken(event.color, event.type) << " " << event.from.row() << " "
        << event.from.col() << " " << event.to.row() << " " << event.to.col() << " "
        << (event.isCapture ? 1 : 0) << " " << event.elapsedMs << " " << event.notation;
    return out.str();
}
```
One line: `MOVE wP 1 0 3 0 0 336 a5` (this is a real example — that's the exact line
captured while testing this session: white pawn, from (1,0) to (3,0), not a capture, 336
simulated ms into the game, chess notation `"a5"`).

`encodePieceCaptured`, `encodeGameStarted`, `encodeGameEnded` follow the identical
pattern — each is 2-6 lines translating one struct's fields into a space-separated line.

**`decode()` — the reverse direction**, and the more interesting half:

```cpp
DecodedMessage decode(const std::string& text) {
    const auto lines = splitLines(text);
    if (lines.empty()) return std::monostate{};

    const auto tokens = BoardParser::split(lines[0]);
    if (tokens.empty()) return std::monostate{};
    const std::string& cmd = tokens[0];

    if (cmd == "CLICK" && tokens.size() >= 3) {
        return ClickMessage{std::stoi(tokens[1]), std::stoi(tokens[2])};
    }
    ...
```
`splitLines` (a small local helper) splits the whole message into trimmed, non-empty
lines. `BoardParser::split` (reused directly, not reimplemented — it's already `public
static` in `Logic/include/io/BoardParser.hpp`) then whitespace-tokenizes the *first*
line to find out which command this is. Every branch below does the same shape of thing:
check the command word, check there are enough tokens to be safe, and construct the
right struct — with one exception:

```cpp
if (cmd == "STATE") {
    StateMessage msg;
    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i] == "END") break;
        const auto fields = BoardParser::split(lines[i]);
        if (fields.size() < 7 || fields[1].size() < 2) continue;  // skip malformed lines
        RenderPiece rp;
        rp.id = static_cast<uintptr_t>(std::stoull(fields[0]));
        rp.color = static_cast<int>(colorFromChar(fields[1][0]));
        rp.type = static_cast<int>(typeFromChar(fields[1][1]));
        rp.x = std::stod(fields[2]);
        rp.y = std::stod(fields[3]);
        rp.state = std::stoi(fields[4]);
        rp.cooldownMs = std::stod(fields[5]);
        rp.cooldownTotalMs = std::stod(fields[6]);
        msg.pieces.push_back(rp);
    }
    return msg;
}
```
Walks every line after `"STATE"` until it hits `"END"`, parsing one `RenderPiece` out of
each. A malformed line is **skipped, not fatal** — `continue` rather than aborting the
whole snapshot — so one bad line can't take down an entire frame's worth of piece
positions.

### `Network/WsServerTransport.hpp` / `.cpp` — the one file allowed to touch `ix::` on the server

Same pattern the project already uses twice: `Img` is the only file allowed to call
`cv::` directly, `SoundPlayer` the only one allowed to call `PlaySoundW`. This is the
third instance of that convention, now for IXWebSocket's server class.

```cpp
class WsServerTransport {
public:
    using ConnectionId = std::string;
    using ConnectHandler = std::function<void(const ConnectionId&)>;
    using MessageHandler = std::function<void(const ConnectionId&, const std::string&)>;
    using DisconnectHandler = std::function<void(const ConnectionId&)>;
    ...
    void send(const ConnectionId& id, const std::string& text);
    void broadcast(const std::string& text);
    void close(const ConnectionId& id);
```
Every connection gets an opaque string ID (IXWebSocket generates one internally per
connection) — that's the *entire* vocabulary `Server/main.cpp` needs: "a connection
showed up," "a connection sent this text," "a connection went away," plus "send/broadcast
some text," "close this one." Nothing about `ix::WebSocket` leaks past this file.

```cpp
void WsServerTransport::start() {
    server_->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket,
               const ix::WebSocketMessagePtr& msg) {
            const std::string id = connectionState->getId();

            if (msg->type == ix::WebSocketMessageType::Open) {
                { std::lock_guard<std::mutex> lock(clientsMutex_); clients_[id] = &webSocket; }
                if (onConnect_) onConnect_(id);
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                if (onMessage_) onMessage_(id, msg->str);
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                { std::lock_guard<std::mutex> lock(clientsMutex_); clients_.erase(id); }
                if (onDisconnect_) onDisconnect_(id);
            }
        });

    if (!server_->listenAndStart()) {
        throw std::runtime_error("WsServerTransport: failed to listen on port " + std::to_string(port_));
    }
}
```
IXWebSocket gives *one* callback that fires for every kind of connection event (open, a
message arriving, close) rather than three separate ones — this function's whole job is
splitting that one callback into the three separate, friendlier callbacks
(`onConnect`/`onMessage`/`onDisconnect`) `Server/main.cpp` actually wants to think in.
`clients_[id] = &webSocket` is how `send()`/`broadcast()` later know which actual
`ix::WebSocket` object to call `.send()` on for a given connection id — a small lookup
table, guarded by `clientsMutex_` because (see below) this callback can fire on more than
one thread at once.

```cpp
void WsServerTransport::broadcast(const std::string& text) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& [id, ws] : clients_) {
        if (ws) ws->send(text);
    }
}
```
Loops every currently-known connection and sends the same text to each — this is what
turns "White made a move" into "both players see it."

### `Network/WsClientTransport.hpp` / `.cpp` — the client-side mirror

Structurally almost the same idea, simpler because a client only ever has *one*
connection (to the server), not many:

```cpp
WsClientTransport::WsClientTransport(const std::string& url) {
    ix::initNetSystem();
    ws_ = std::make_unique<ix::WebSocket>();
    ws_->setUrl(url);
    ws_->enableAutomaticReconnection();
}
```
`ix::initNetSystem()` does Windows-specific socket setup (`WSAStartup`, if you've seen
that before) that has to happen once before any socket code runs at all —
`WsServerTransport` does the equivalent in its own constructor.
`enableAutomaticReconnection()` is IXWebSocket's own built-in retry loop — if the
connection drops (or never succeeds in the first place), it keeps quietly retrying
forever in the background, no code in this project has to implement that itself. That's
also exactly why a client pointed at a wrong/unreachable server just sits there silently
forever rather than failing loudly — a real usability gap worth fixing in a later phase
(add a timeout and an on-screen error), not something this phase solved.

---

## 4. `Server/main.cpp` — the whole server, piece by piece

This is the new "composition root" for the server side — the direct sibling of what
`main.cpp` used to be, just without any UI.

```cpp
auto ruleEngine = std::make_shared<RuleEngine>(board);
auto game = std::make_shared<GameEngine>(board, ruleEngine);
```
Identical to how the old local `main.cpp` built things. This line is the whole reason
Phase 1 needed **zero changes** inside `Logic/` — the server is just a new *caller* of
the exact same objects.

```cpp
struct PlayerSession {
    PlayerColor color;
    std::shared_ptr<Controller> controller;
};
```
One of these per connected player. Notice it holds a *whole `Controller`*, not just a
color — this is the detail that makes the rest of the file simple: `Controller` was
already designed (long before networking was ever discussed) to own nothing but its own
`selectedPosition_` and delegate everything else to whichever `GameEngine` it's handed.
That meant giving White and Black *separate* `Controller`s, both pointed at the *one*
shared `GameEngine`, worked immediately, with no `Logic/` changes — each player's
"what did I select" state stays private to their own `Controller`, while the actual board
they're both looking at is the same one.

```cpp
std::mutex gameMutex;
```
The one genuinely new concept in this whole phase. Explained fully in section 6, but the
short version: IXWebSocket calls this program's callbacks (`onConnect`/`onMessage`/etc.)
from its *own* background threads, not from `main()`'s thread — and `GameEngine` was
never written to be safe to touch from two threads at once. `gameMutex` is locked around
*every* place this file touches `game`/`sessions`, so only one thread is ever actually
inside `GameEngine` at a time, no matter how many connections are talking at once.

```cpp
server.onConnect([&](const WsServerTransport::ConnectionId& id) {
    std::lock_guard<std::mutex> lock(gameMutex);

    if (sessions.size() >= 2) {
        std::cout << "[server] rejecting extra connection " << id << " ..." << std::endl;
        server.close(id);
        return;
    }

    const PlayerColor color = sessions.empty() ? PlayerColor::White : PlayerColor::Black;
    sessions[id] = PlayerSession{color, std::make_shared<Controller>(game)};
    server.send(id, encodeWelcome(color));

    if (sessions.size() == 2) {
        game->start();
    }
});
```
Whoever connects **first** becomes White, whoever connects **second** becomes Black —
`sessions.empty()` is the only thing deciding that, which is why it matters which
`kungfu_app.exe` window you launch first when testing. A third connection gets closed
immediately (`server.close(id)`) rather than silently ignored, so a third client's window
doesn't just hang forever with no explanation (spectators, which *would* want a third
connection to succeed, are an explicitly later phase). The moment the second player
connects, `game->start()` fires — this is also the moment `GameStarted` gets published on
`GameEngine`'s own bus (built the session before this one), which is what makes the
`"GAME START"` banner show up on both windows at the same instant.

```cpp
server.onMessage([&](const WsServerTransport::ConnectionId& id, const std::string& text) {
    std::lock_guard<std::mutex> lock(gameMutex);

    auto it = sessions.find(id);
    if (it == sessions.end()) return;  // a rejected connection - ignore whatever it sends

    const auto decoded = decode(text);
    const auto* click = std::get_if<ClickMessage>(&decoded);
    if (!click) return;

    PlayerSession& session = it->second;
    const Position cell(click->row, click->col);

    if (!session.controller->hasSelection()) {
        const auto piece = game->getBoard()->pieceAt(cell);
        if (!piece.has_value() || !piece.value() || piece.value()->color() != session.color) {
            return;
        }
    }

    session.controller->handleCellClick(cell.row(), cell.col());
});
```
This is the **one new rule that doesn't exist anywhere locally** — in the old one-mouse
app, either color could always be clicked, because there was no concept of "whose click
is this." Here: if this connection doesn't currently have a piece selected, the clicked
square has to hold *that connection's own color*, or the click is silently dropped
(`return`, no error message back — same "just doesn't happen" behavior the old local
`Controller` already had for an illegal click). If a selection *is* already active, it
was already checked at that point in time, so the second click (the actual move/jump
target) is forwarded straight through — exactly mirroring how the old local
`Controller::handleCellClick`'s own two-click state machine already worked, just with one
extra gate in front of the "select" half.

```cpp
game->onMoveStarted().subscribe(
    [&](const MoveStarted& event) { server.broadcast(encodeMoveStarted(event)); });
game->onPieceCaptured().subscribe(
    [&](const PieceCaptured& event) { server.broadcast(encodePieceCaptured(event)); });
game->onGameStarted().subscribe([&](const GameStarted&) { server.broadcast(encodeGameStarted()); });
game->onGameEnded().subscribe(
    [&](const GameEnded& event) { server.broadcast(encodeGameEnded(event)); });
```
Four lines, and this is the entire bridge between "the event bus built last session" and
"the network." Each of `GameEngine`'s four buses already existed for the local UI to
subscribe to (score panel, sound, banners); the server just adds a *second* kind of
subscriber to those same buses — one that encodes the event as text and broadcasts it,
instead of playing a sound or updating a panel directly. Neither `GameEngine` nor
`RealTimeArbiter` had to change even slightly to support this — they already didn't care
*who* was listening.

```cpp
while (true) {
    { std::lock_guard<std::mutex> lock(gameMutex); game->wait(kTickMs); }
    broadcastState();
    std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
}
```
The server's own heartbeat, replacing what used to be `main.cpp`'s render loop calling
`controller->handleTimePassed(16)` every frame. Same 16ms cadence, same
`GameEngine::wait()` call — just with nothing drawn afterward, and a `STATE` broadcast
sent to both connections instead.

---

## 5. The client side

### `UI/NetClient/RemoteGameProxy.hpp` / `.cpp` — a `GameEngine`-shaped stand-in

The design goal stated up front in the header comment: **mirror `GameEngine`'s public
surface closely enough that `main.cpp`'s existing subscriber-wiring code barely
changes.**

```cpp
EventBus<MoveStarted>& onMoveStarted() { return moveBus_; }
EventBus<PieceCaptured>& onPieceCaptured() { return captureBus_; }
EventBus<GameStarted>& onGameStarted() { return startBus_; }
EventBus<GameEnded>& onGameEnded() { return endBus_; }
```
Identical method names and return types to `GameEngine`'s own. `main.cpp`'s subscriber
code from the previous session (`game->onMoveStarted().subscribe(...)`) became
`proxy.onMoveStarted().subscribe(...)` and nothing else about those four blocks changed
at all.

The trickiest part of this file is **why it needs a queue instead of just publishing
immediately**:

```cpp
void RemoteGameProxy::handleMessage(const std::string& text) {
    const auto decoded = net::decode(text);
    ...
    } else if (const auto* move = std::get_if<MoveStarted>(&decoded)) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        pendingEvents_.push_back(*move);
    }
    ...
}
```
`handleMessage` runs on IXWebSocket's own background thread (the same kind of thread
`WsServerTransport` uses on the server side). If it called `moveBus_.publish(*move)`
*directly* from there, every subscriber — `SoundPlayer::play`, the code that appends to
`scoreboard.white.moves`, the banner tracker — would *also* run on that background
thread, at the same time `main()`'s render loop (on the *main* thread) might be reading
`scoreboard` to draw a frame. Two threads touching the same data with no coordination is
exactly the kind of bug that only shows up sometimes, unpredictably — so instead,
`handleMessage` just appends the decoded event to a small, mutex-protected queue and
returns immediately.

```cpp
void RemoteGameProxy::pollEvents() {
    std::vector<QueuedEvent> events;
    { std::lock_guard<std::mutex> lock(queueMutex_); events.swap(pendingEvents_); }

    for (const auto& event : events) {
        std::visit([this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, MoveStarted>) moveBus_.publish(e);
            else if constexpr (std::is_same_v<T, PieceCaptured>) captureBus_.publish(e);
            else if constexpr (std::is_same_v<T, GameStarted>) startBus_.publish(e);
            else if constexpr (std::is_same_v<T, GameEnded>) endBus_.publish(e);
        }, event);
    }
}
```
`main()`'s render loop calls this **once per frame, on the main thread** (`proxy.pollEvents();`
is the very first line inside `while (view.isOpen())` in `main.cpp`). It grabs whatever
arrived since the last frame (`events.swap(pendingEvents_)` — a cheap way to both copy
the queue out and clear it in one step) and *only then* actually calls `.publish()` on
each bus — meaning every subscriber (sound, score panel, banner) still only ever runs on
the main thread, exactly like before networking existed. `std::visit` +
`if constexpr` is how C++ dispatches on "which type is actually inside this
`std::variant` right now" at compile time — for each possible type `T` the variant could
hold, exactly one of the four `if constexpr` branches actually gets compiled in for that
case.

`getRenderState()`, `myColor()`, `hasColor()` are simpler — they're just snapshots of
plain data (the latest `STATE` received, the color from `WELCOME`), each behind its own
tiny mutex, safe to call from the main thread at any time without needing the queue
dance.

### The changes to `main.cpp` itself

Everything removed: `BoardParser`, `Board`, `RuleEngine`, `GameEngine`, `Controller` — the
client doesn't construct *any* of these anymore. That's not a simplification for its own
sake; it's the direct, literal meaning of "the client no longer owns game logic."

Everything added:

- **`RemoteGameProxy proxy(serverUrl);`** — replaces the old
  `GameEngine`+`RuleEngine`+`Controller` construction as the one thing `main.cpp` builds
  to talk to the game.
- **The connect-and-wait loop:**
  ```cpp
  while (!proxy.hasColor()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ```
  Blocks startup until the `WELCOME` message has actually arrived — there's no sensible
  window to draw before the client even knows which color it is.
- **`BoardClickHandler` got simpler and gained one new cosmetic-only job.** It used to
  hold a `Controller&` and call `controller_.handleCellClick(...)`; now it holds a
  `RemoteGameProxy&` and calls `proxy_.sendClick(...)` instead — the click leaves the
  building immediately rather than being processed locally. It also now tracks
  `localSelection_` itself:
  ```cpp
  if (localSelection_.has_value()) {
      localSelection_.reset();
  } else {
      localSelection_ = cell;
  }
  ```
  This is a deliberately simple, **optimistic** echo of the old local selection
  highlight: first click after none selected → show it selected; any click after that →
  clear it. It doesn't know or care whether the server actually accepted the click as
  legal — it's purely cosmetic, and the plan going in explicitly accepted that it might
  occasionally flash on a click the server silently rejects. The *real* authority (what
  actually moved) still comes from the server's `MOVE` broadcast, handled completely
  separately (next point).
- **The last-move highlight moved from being click-driven to being event-driven.** Before
  networking, `BoardClickHandler` itself tracked "was the most recent click a completed
  move" by comparing `Controller::selectedPosition()` before and after. That information
  doesn't exist on the client anymore (only the server's `Controller`s know it) — so
  instead, a new subscriber does it properly, from the authoritative source:
  ```cpp
  proxy.onMoveStarted().subscribe([&](const MoveStarted& event) {
      ...
      lastMove.show({event.from, event.to}, kLastMoveHighlightDuration);
      ...
  });
  ```
  This is arguably *more* correct than the old local version — it's driven by "a move
  genuinely happened," not by "two clicks occurred in a row locally."
- **`Expiring<T>`** is a small new template class replacing what used to be two
  almost-identical hand-written trackers (the old code had a `BannerTracker` class and a
  `recentMove()` method doing the same "remember a value, forget it after N
  milliseconds" shape, slightly differently written each time). Now there's one generic
  version, used three times (`Expiring<std::string>` for the banner text,
  `Expiring<std::pair<Position, Position>>` for the last-move squares) — the ordinary
  kind of small dedup this project has favored throughout.

---

## 6. `run.bat` / `run_server.bat`

`run_server.bat` is new, and deliberately does **not** set `PATH` for OpenCV at all —
the whole point being demonstrated by its existence is that the server genuinely doesn't
need it. `run.bat` (the client) gained one line:
```bat
build\kungfu_app.exe %1
```
`%1` forwards whatever first argument the script itself was called with straight through
to the exe — this is what makes `run.bat 192.168.1.42` work for connecting to a server on
another machine, versus `run.bat` alone defaulting to `127.0.0.1`.

---

## 7. From a home network to a genuinely large network

Everything above was built and tested on one LAN (same Wi-Fi router, or the same
machine). Here's honestly what does and doesn't already generalize toward "many players,
possibly far apart, over the real internet":

**Already the right shape, no rework needed later:**
- The *message-based* design. A server broadcasting discrete text events instead of
  clients polling shared memory is exactly the same shape whether the other end is on
  the same desk or on another continent — WebSocket doesn't care about distance, only
  about latency (which just means moves take a little longer to show up, not that
  anything breaks).
- The mutex-guarded single `GameEngine` per match. This is already "one match, isolated
  state" — the shape a later phase needs to run *many* matches side by side (one
  `PlayerSession`-style bundle + mutex per room, instead of one global pair).
- `Controller` supporting multiple independent instances sharing one `GameEngine` — this
  is what will let a third, fourth, fifth connection (spectators) attach to the same
  match later with no `Logic/` changes, the same way the second player already did.

**Would need real work before this is safe to expose past a trusted home network:**
- **No encryption.** `ws://` is plain text — anyone who can see the network traffic can
  read every move. The real internet needs `wss://` (the `USE_TLS OFF` CMake flag from
  section 2 flipped back on, plus a TLS certificate) before this should ever cross a
  network you don't fully trust.
- **No authentication at all.** Right now, whoever connects first *is* White — there's
  no check that it's actually you. This is explicitly what the next phase (username,
  eventually password) exists to fix.
- **No port-forwarding/NAT handling.** Two computers on the same Wi-Fi can reach each
  other directly by local IP; two computers on two different home internet connections
  generally can't, without the server's router being configured to forward port 7777 in
  from the outside (or the server being hosted somewhere already internet-reachable,
  like a small cloud VM — a much more common real-world approach than port-forwarding a
  home router).
- **One process, one match (well, two players).** The concurrency model here is
  deliberately the simplest thing that works for exactly one match. "Many concurrent
  matches, many players" is explicitly the later, separate scaling phase from the
  roadmap — this phase's job was proving the client/server split itself works at all,
  not handling load.

None of that is a flaw in what got built — it's the honest scope boundary of "prove two
players can play a real match over a network," which is what this phase set out to do
and what the earlier hands-on testing (both scripted and with two real windows) confirmed
actually works.

---

## 8. Quick reference — every file this phase touched

| File | New or changed | What it is |
|---|---|---|
| `CMakeLists.txt` | changed | Library split (`kungfu_logic`/`kungfu_ui`/`kungfu_network`), IXWebSocket fetch, new executables |
| `Network/Protocol.hpp`/`.cpp` | new | Message types + encode/decode |
| `Network/WsServerTransport.hpp`/`.cpp` | new | Server-side WebSocket wrapper |
| `Network/WsClientTransport.hpp`/`.cpp` | new | Client-side WebSocket wrapper |
| `Server/main.cpp` | new | The `kungfu_server` executable |
| `UI/NetClient/RemoteGameProxy.hpp`/`.cpp` | new | Client-side `GameEngine`-shaped stand-in |
| `main.cpp` | changed | No longer owns game logic; talks to `RemoteGameProxy` instead |
| `run.bat` | changed | Forwards an optional server-address argument |
| `run_server.bat` | new | Launches the server (no OpenCV `PATH` needed) |

Nothing under `Logic/` changed in this phase at all — the full existing test suite
(254 unit + 12 integration assertions) passed identically before and after.
