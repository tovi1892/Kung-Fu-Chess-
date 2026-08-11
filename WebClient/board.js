import { PieceState, pieceGlyph } from "./protocol.js";

// Canvas board renderer. Thin-client philosophy (see Client/RemoteGameProxy.hpp): this only
// ever draws whatever the last STATE snapshot said, plus purely-local UI state (selection/
// last-move highlight) - there is no client-side rules engine here.
//
// Board orientation invariant (see CLAUDE.md): White advances row+1/starts row 1, Black
// row-1/starts row 6 - the opposite of the usual "White at the bottom" diagram. Row 0 is
// drawn at the BOTTOM of the canvas so White's own pieces start near the viewer.
export class Board {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");
    this.cellSize = canvas.width / 8;
    this.pieces = [];
    this.stateReceivedAt = performance.now();
    this.selection = null; // { row, col } | null
    this.lastMove = null; // { fromRow, fromCol, toRow, toCol } | null

    this._raf = null;
  }

  setState(pieces) {
    this.pieces = pieces;
    this.stateReceivedAt = performance.now();
  }

  setSelection(rowCol) {
    this.selection = rowCol;
  }

  setLastMove(move) {
    this.lastMove = move;
  }

  // screen pixel -> board {row, col}, independent of the desktop app's fixed CELL_SIZE=100
  // convention - the wire CLICK message only ever carries row/col, never raw pixels.
  pixelToCell(px, py) {
    const col = Math.floor(px / this.cellSize);
    const row = 7 - Math.floor(py / this.cellSize);
    if (row < 0 || row > 7 || col < 0 || col > 7) return null;
    return { row, col };
  }

  cellCenterPixel(row, col) {
    return {
      x: (col + 0.5) * this.cellSize,
      y: (7 - row + 0.5) * this.cellSize,
    };
  }

  start() {
    const loop = () => {
      this._render();
      this._raf = requestAnimationFrame(loop);
    };
    this._raf = requestAnimationFrame(loop);
  }

  stop() {
    if (this._raf) cancelAnimationFrame(this._raf);
    this._raf = null;
  }

  _render() {
    const { ctx, cellSize } = this;
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    for (let row = 0; row < 8; row++) {
      for (let col = 0; col < 8; col++) {
        const dark = (row + col) % 2 === 0;
        ctx.fillStyle = dark ? "#7d5a44" : "#eadfce";
        const px = col * cellSize;
        const py = (7 - row) * cellSize;
        ctx.fillRect(px, py, cellSize, cellSize);
      }
    }

    if (this.lastMove) {
      ctx.fillStyle = "rgba(255, 214, 0, 0.35)";
      for (const [r, c] of [
        [this.lastMove.fromRow, this.lastMove.fromCol],
        [this.lastMove.toRow, this.lastMove.toCol],
      ]) {
        ctx.fillRect(c * cellSize, (7 - r) * cellSize, cellSize, cellSize);
      }
    }

    if (this.selection) {
      ctx.fillStyle = "rgba(50, 160, 255, 0.4)";
      ctx.fillRect(this.selection.col * cellSize, (7 - this.selection.row) * cellSize, cellSize, cellSize);
    }

    const elapsed = performance.now() - this.stateReceivedAt;
    for (const piece of this.pieces) {
      if (piece.state === PieceState.Captured) continue;
      this._drawPiece(piece, elapsed);
    }
  }

  _drawPiece(piece, elapsedSinceState) {
    const { ctx, cellSize } = this;
    const cx = (piece.x + 0.5) * cellSize;
    const cy = (7 - piece.y + 0.5) * cellSize;

    if (piece.state === PieceState.Airborne) {
      ctx.save();
      ctx.shadowColor = "rgba(120, 200, 255, 0.9)";
      ctx.shadowBlur = 18;
    }

    ctx.font = `${cellSize * 0.72}px "Segoe UI Symbol", sans-serif`;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillStyle = piece.color === "w" ? "#ffffff" : "#1a1a1a";
    ctx.strokeStyle = piece.color === "w" ? "#333333" : "#dddddd";
    ctx.lineWidth = 1.5;
    const glyph = pieceGlyph(piece.color, piece.type);
    ctx.strokeText(glyph, cx, cy);
    ctx.fillText(glyph, cx, cy);

    if (piece.state === PieceState.Airborne) {
      ctx.restore();
    }

    if (piece.state === PieceState.Cooldown && piece.cooldownTotalMs > 0) {
      const remainingMs = Math.max(0, piece.cooldownMs - elapsedSinceState);
      const fraction = Math.min(1, remainingMs / piece.cooldownTotalMs);
      this._drawCooldownRing(cx, cy, cellSize * 0.42, fraction);
    } else if (piece.state === PieceState.ShortRest) {
      this._drawCooldownRing(cx, cy, cellSize * 0.42, 0.5, "rgba(160, 255, 160, 0.85)");
    }
  }

  _drawCooldownRing(cx, cy, radius, fraction, color = "rgba(255, 120, 120, 0.85)") {
    const { ctx } = this;
    ctx.save();
    ctx.strokeStyle = "rgba(0, 0, 0, 0.25)";
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, Math.PI * 2);
    ctx.stroke();

    ctx.strokeStyle = color;
    ctx.lineWidth = 3;
    ctx.beginPath();
    const start = -Math.PI / 2;
    ctx.arc(cx, cy, radius, start, start + fraction * Math.PI * 2);
    ctx.stroke();
    ctx.restore();
  }
}
