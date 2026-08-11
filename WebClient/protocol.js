// JS mirror of Network/Protocol.hpp/.cpp - the wire format lives in exactly one place on the
// C++ side, this is the browser-side equivalent of that same text protocol. Keep in sync with
// Protocol.cpp by hand; there is no shared source of truth across the language boundary.

export const PieceState = {
  Idle: 0,
  Moving: 1,
  Cooldown: 2,
  Airborne: 3,
  ShortRest: 4,
  Captured: 5,
};

const PIECE_GLYPH = {
  K: { w: "♔", b: "♚" },
  Q: { w: "♕", b: "♛" },
  R: { w: "♖", b: "♜" },
  B: { w: "♗", b: "♝" },
  N: { w: "♘", b: "♞" },
  P: { w: "♙", b: "♟" },
};

export function pieceGlyph(color, type) {
  return PIECE_GLYPH[type][color];
}

// Mirrors Logic/history/GameRecord.cpp's pieceValue() - knight/bishop=3, rook=5, queen=9,
// pawn=1, king=0 (capturing a king ends the game before a score would matter).
const PIECE_VALUE = { K: 0, Q: 9, R: 5, B: 3, N: 3, P: 1 };

export function pieceValue(type) {
  return PIECE_VALUE[type] ?? 0;
}

export function encodeLogin(username, password) {
  return `LOGIN ${username} ${password}`;
}

// mode: "Q" | "C" | "R". room only meaningful for "R".
export function encodeJoin(mode, room = "") {
  return mode === "R" ? `JOIN R ${room}` : `JOIN ${mode}`;
}

export function encodeClick(row, col) {
  return `CLICK ${row} ${col}`;
}

function splitTokens(line) {
  return line.trim().split(/\s+/).filter((t) => t.length > 0);
}

function colorFromChar(c) {
  return c === "w" ? "w" : "b";
}

// Returns { type: "<MessageType>", ...fields } for every message Protocol.cpp's decode()
// recognizes, or { type: "unknown" } for anything malformed/unrecognized (mirrors decode()
// returning std::monostate).
export function decode(text) {
  const lines = text.split("\n").map((l) => l.trim()).filter((l) => l.length > 0);
  if (lines.length === 0) {
    return { type: "unknown" };
  }
  const tokens = splitTokens(lines[0]);
  if (tokens.length === 0) {
    return { type: "unknown" };
  }
  const cmd = tokens[0];

  switch (cmd) {
    case "LOGIN_OK":
      if (tokens.length >= 3) {
        return { type: "LoginOk", rating: parseInt(tokens[1], 10), accountCreated: tokens[2] === "1" };
      }
      break;
    case "LOGIN_FAIL":
      if (tokens.length >= 2) {
        return { type: "LoginFail", reason: tokens[1] };
      }
      break;
    case "WELCOME":
      if (tokens.length >= 2 && tokens[1]) {
        return { type: "Welcome", color: colorFromChar(tokens[1][0]) };
      }
      break;
    case "SPECTATE":
      return { type: "Spectate" };
    case "ROOM":
      if (tokens.length >= 2) {
        return { type: "Room", key: tokens[1] };
      }
      break;
    case "PLAYERS":
      if (tokens.length >= 3) {
        return { type: "Players", white: tokens[1], black: tokens[2] };
      }
      break;
    case "NO_OPPONENT":
      return { type: "NoOpponent" };
    case "FORFEIT_WARNING":
      if (tokens.length >= 3 && tokens[1]) {
        return { type: "ForfeitWarning", color: colorFromChar(tokens[1][0]), graceMs: parseInt(tokens[2], 10) };
      }
      break;
    case "FORFEIT":
      if (tokens.length >= 2 && tokens[1]) {
        return { type: "Forfeit", winner: colorFromChar(tokens[1][0]) };
      }
      break;
    case "RECONNECTED":
      if (tokens.length >= 2 && tokens[1]) {
        return { type: "Reconnected", color: colorFromChar(tokens[1][0]) };
      }
      break;
    case "RATINGS":
      if (tokens.length >= 3) {
        return { type: "Ratings", white: parseInt(tokens[1], 10), black: parseInt(tokens[2], 10) };
      }
      break;
    case "STATE": {
      const pieces = [];
      for (let i = 1; i < lines.length; i++) {
        if (lines[i] === "END") break;
        const fields = splitTokens(lines[i]);
        if (fields.length < 7 || fields[1].length < 2) continue;
        pieces.push({
          id: fields[0],
          color: colorFromChar(fields[1][0]),
          type: fields[1][1],
          x: parseFloat(fields[2]),
          y: parseFloat(fields[3]),
          state: parseInt(fields[4], 10),
          cooldownMs: parseFloat(fields[5]),
          cooldownTotalMs: parseFloat(fields[6]),
        });
      }
      return { type: "State", pieces };
    }
    case "MOVE":
      if (tokens.length >= 9 && tokens[1] && tokens[1].length >= 2) {
        return {
          type: "Move",
          color: colorFromChar(tokens[1][0]),
          pieceType: tokens[1][1],
          fromRow: parseInt(tokens[2], 10),
          fromCol: parseInt(tokens[3], 10),
          toRow: parseInt(tokens[4], 10),
          toCol: parseInt(tokens[5], 10),
          isCapture: tokens[6] === "1",
          elapsedMs: parseInt(tokens[7], 10),
          notation: tokens[8],
        };
      }
      break;
    case "CAPTURE":
      if (tokens.length >= 5 && tokens[1] && tokens[2]) {
        return {
          type: "Capture",
          capturingColor: colorFromChar(tokens[1][0]),
          capturedType: tokens[2][0],
          row: parseInt(tokens[3], 10),
          col: parseInt(tokens[4], 10),
        };
      }
      break;
    case "GAME_START":
      return { type: "GameStarted" };
    case "GAME_END":
      if (tokens.length >= 2 && tokens[1]) {
        return { type: "GameEnded", winner: colorFromChar(tokens[1][0]) };
      }
      break;
  }
  return { type: "unknown" };
}
