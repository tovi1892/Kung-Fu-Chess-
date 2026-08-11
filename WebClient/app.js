import * as proto from "./protocol.js";
import { Connection } from "./network.js";
import { Board } from "./board.js";

const DEFAULT_SERVER_URL = "wss://kung-fu-chess.onrender.com";
const STORAGE_KEY_URL = "kfc.serverUrl";
const STORAGE_KEY_NAME = "kfc.username";

const el = (id) => document.getElementById(id);

const lobbyEl = el("lobby");
const gameEl = el("game");
const statusBadge = el("statusBadge");
const serverUrlInput = el("serverUrl");
const usernameInput = el("username");
const passwordInput = el("password");
const roomCodeInput = el("roomCodeInput");
const loginError = el("loginError");
const btnQuickMatch = el("btnQuickMatch");
const btnCreateRoom = el("btnCreateRoom");
const btnJoinRoom = el("btnJoinRoom");

const canvas = el("boardCanvas");
const roomLabel = el("roomLabel");
const whiteName = el("whiteName");
const blackName = el("blackName");
const whiteRating = el("whiteRating");
const blackRating = el("blackRating");
const whiteScore = el("whiteScore");
const blackScore = el("blackScore");
const moveLog = el("moveLog");
const banner = el("banner");
const btnLeave = el("btnLeave");

const conn = new Connection();
const board = new Board(canvas);
board.start();

const state = {
  pendingJoin: null, // { mode, room } - sent once LOGIN_OK arrives
  myColor: null, // "w" | "b" | null (spectator)
  isSpectator: false,
  scores: { w: 0, b: 0 },
};

function setStatus(status) {
  statusBadge.textContent = {
    connecting: "Connecting…",
    connected: "Connected",
    disconnected: "Disconnected",
    error: "Connection error",
  }[status] ?? status;
  statusBadge.className = `status status-${status}`;
}

conn.onStatus = setStatus;
conn.onMessage = handleMessage;

function showLobby(errorText) {
  lobbyEl.classList.remove("hidden");
  gameEl.classList.add("hidden");
  loginError.textContent = errorText ?? "";
  setButtonsEnabled(true);
}

function showGame() {
  lobbyEl.classList.add("hidden");
  gameEl.classList.remove("hidden");
}

function setButtonsEnabled(enabled) {
  btnQuickMatch.disabled = !enabled;
  btnCreateRoom.disabled = !enabled;
  btnJoinRoom.disabled = !enabled;
}

function startLogin(pendingJoin) {
  const url = serverUrlInput.value.trim() || DEFAULT_SERVER_URL;
  const username = usernameInput.value.trim();
  const password = passwordInput.value;
  if (!username || !password) {
    loginError.textContent = "Enter a name and password first.";
    return;
  }
  localStorage.setItem(STORAGE_KEY_URL, url);
  localStorage.setItem(STORAGE_KEY_NAME, username);

  state.pendingJoin = pendingJoin;
  setButtonsEnabled(false);
  loginError.textContent = "";

  conn.connect(url);
  // LOGIN is sent the instant the connection opens - onStatus fires "connected" first, but
  // we hook the actual send off that same transition here rather than a second listener.
  const trySend = () => {
    if (conn.status === "connected") {
      conn.send(proto.encodeLogin(username, password));
    } else if (conn.status === "error" || conn.status === "disconnected") {
      showLobby("Could not reach the server.");
    } else {
      setTimeout(trySend, 50);
    }
  };
  trySend();
}

btnQuickMatch.addEventListener("click", () => startLogin({ mode: "Q" }));
btnCreateRoom.addEventListener("click", () => startLogin({ mode: "C" }));
btnJoinRoom.addEventListener("click", () => {
  const room = roomCodeInput.value.trim();
  if (!room) {
    loginError.textContent = "Enter a room code to join.";
    return;
  }
  startLogin({ mode: "R", room });
});

btnLeave.addEventListener("click", () => {
  conn.close();
  resetGameUi();
  showLobby();
});

function resetGameUi() {
  state.myColor = null;
  state.isSpectator = false;
  state.scores = { w: 0, b: 0 };
  board.setState([]);
  board.setSelection(null);
  board.setLastMove(null);
  moveLog.innerHTML = "";
  roomLabel.textContent = "";
  whiteName.textContent = "White";
  blackName.textContent = "Black";
  whiteRating.textContent = "";
  blackRating.textContent = "";
  whiteScore.textContent = "0";
  blackScore.textContent = "0";
  hideBanner();
}

function showBanner(text) {
  banner.textContent = text;
  banner.classList.remove("hidden");
}
function hideBanner() {
  banner.classList.add("hidden");
}

function handleMessage(text) {
  const msg = proto.decode(text);
  switch (msg.type) {
    case "LoginOk": {
      const join = state.pendingJoin;
      state.pendingJoin = null;
      conn.send(proto.encodeJoin(join.mode, join.room ?? ""));
      break;
    }
    case "LoginFail":
      state.pendingJoin = null;
      conn.close();
      showLobby(`Login failed: ${msg.reason}`);
      break;
    case "Welcome":
      state.myColor = msg.color;
      state.isSpectator = false;
      showGame();
      break;
    case "Spectate":
      state.myColor = null;
      state.isSpectator = true;
      showGame();
      break;
    case "Room":
      roomLabel.textContent = `Room code: ${msg.key}`;
      break;
    case "Players":
      whiteName.textContent = msg.white;
      blackName.textContent = msg.black;
      break;
    case "NoOpponent":
      showLobby("No opponent found in time - try again.");
      conn.close();
      break;
    case "GameStarted":
      hideBanner();
      break;
    case "State":
      board.setState(msg.pieces);
      break;
    case "Move":
      addMoveLogEntry(msg);
      board.setLastMove({ fromRow: msg.fromRow, fromCol: msg.fromCol, toRow: msg.toRow, toCol: msg.toCol });
      break;
    case "Capture": {
      const value = proto.pieceValue(msg.capturedType);
      state.scores[msg.capturingColor] += value;
      whiteScore.textContent = String(state.scores.w);
      blackScore.textContent = String(state.scores.b);
      break;
    }
    case "ForfeitWarning":
      showBanner(
        `${msg.color === "w" ? whiteName.textContent : blackName.textContent} disconnected - ` +
          `forfeiting in ${Math.round(msg.graceMs / 1000)}s unless they return`
      );
      break;
    case "Reconnected":
      hideBanner();
      break;
    case "Forfeit":
      showBanner(`${msg.winner === "w" ? "White" : "Black"} wins by forfeit`);
      break;
    case "GameEnded":
      showBanner(`${msg.winner === "w" ? "White" : "Black"} wins!`);
      break;
    case "Ratings":
      whiteRating.textContent = `(${msg.white})`;
      blackRating.textContent = `(${msg.black})`;
      break;
    default:
      break;
  }
}

function addMoveLogEntry(move) {
  const row = document.createElement("li");
  row.className = move.color === "w" ? "move-white" : "move-black";
  row.textContent = `${move.color === "w" ? "White" : "Black"}: ${move.notation}`;
  moveLog.appendChild(row);
  moveLog.scrollTop = moveLog.scrollHeight;
}

// --- Board interaction: click-to-select-then-click-to-move, plus a drag gesture that fires
// the same two CLICK messages atomically (mousedown = first click, mouseup = second). ---

let pointerDownCell = null;

function canAct() {
  return !state.isSpectator && state.myColor !== null;
}

canvas.addEventListener("pointerdown", (ev) => {
  if (!canAct()) return;
  const rect = canvas.getBoundingClientRect();
  const px = ((ev.clientX - rect.left) / rect.width) * canvas.width;
  const py = ((ev.clientY - rect.top) / rect.height) * canvas.height;
  pointerDownCell = board.pixelToCell(px, py);
});

canvas.addEventListener("pointerup", (ev) => {
  if (!canAct() || !pointerDownCell) {
    pointerDownCell = null;
    return;
  }
  const rect = canvas.getBoundingClientRect();
  const px = ((ev.clientX - rect.left) / rect.width) * canvas.width;
  const py = ((ev.clientY - rect.top) / rect.height) * canvas.height;
  const upCell = board.pixelToCell(px, py);
  const downCell = pointerDownCell;
  pointerDownCell = null;
  if (!upCell) return;

  const isDrag = downCell && (downCell.row !== upCell.row || downCell.col !== upCell.col);

  if (isDrag) {
    conn.send(proto.encodeClick(downCell.row, downCell.col));
    conn.send(proto.encodeClick(upCell.row, upCell.col));
    board.setSelection(null);
    return;
  }

  if (!board.selection) {
    conn.send(proto.encodeClick(upCell.row, upCell.col));
    board.setSelection(upCell);
  } else {
    conn.send(proto.encodeClick(upCell.row, upCell.col));
    board.setSelection(null);
  }
});

// --- Prefill from a shared room link (?room=CODE&server=URL), and remembered name/URL. ---

(function init() {
  const params = new URLSearchParams(location.search);
  const sharedRoom = params.get("room");
  const sharedServer = params.get("server");

  serverUrlInput.value = sharedServer || localStorage.getItem(STORAGE_KEY_URL) || DEFAULT_SERVER_URL;
  usernameInput.value = localStorage.getItem(STORAGE_KEY_NAME) || "";
  if (sharedRoom) {
    roomCodeInput.value = sharedRoom;
  }
  setStatus("disconnected");
  showLobby();
})();
