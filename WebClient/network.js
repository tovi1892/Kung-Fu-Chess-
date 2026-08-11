// Thin WebSocket wrapper: connection lifecycle + status callbacks for the status badge, and a
// send() that's a no-op (not queued - the lobby already blocks input until the socket is open
// and LOGIN_OK has arrived, so there's nothing meaningful to buffer) if called too early.

export class Connection {
  constructor() {
    this.ws = null;
    this.status = "disconnected"; // "connecting" | "connected" | "disconnected" | "error"
    this.onStatus = null; // (status) => void
    this.onMessage = null; // (text) => void
  }

  connect(url) {
    this.close();
    this._setStatus("connecting");
    const ws = new WebSocket(url);
    this.ws = ws;
    ws.addEventListener("open", () => this._setStatus("connected"));
    ws.addEventListener("message", (ev) => {
      if (this.onMessage) this.onMessage(ev.data);
    });
    ws.addEventListener("close", () => this._setStatus("disconnected"));
    ws.addEventListener("error", () => this._setStatus("error"));
  }

  send(text) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(text);
    }
  }

  close() {
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  _setStatus(status) {
    this.status = status;
    if (this.onStatus) this.onStatus(status);
  }
}
