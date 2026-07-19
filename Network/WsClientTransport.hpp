#pragma once

#include <functional>
#include <memory>
#include <string>

namespace ix {
class WebSocket;
}  // namespace ix

namespace kungfu::net {

// Client-side counterpart to WsServerTransport - the one place allowed to touch ix::
// directly on the client. Callbacks fire on IXWebSocket's own background thread; the
// client side has no shared mutable game state of its own to protect (RemoteGameProxy
// only ever appends to its own EventBuses / overwrites its own latest-state snapshot).
class WsClientTransport {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    using OpenHandler = std::function<void()>;

    explicit WsClientTransport(const std::string& url);
    ~WsClientTransport();

    // Handlers must be set before start().
    void onMessage(MessageHandler handler);
    void onOpen(OpenHandler handler);

    // Connects asynchronously - IXWebSocket runs its own I/O thread and reconnects
    // automatically if the connection drops.
    void start();
    void stop();

    void send(const std::string& text);

private:
    std::unique_ptr<ix::WebSocket> ws_;
    MessageHandler onMessage_;
    OpenHandler onOpen_;
};

}  // namespace kungfu::net
