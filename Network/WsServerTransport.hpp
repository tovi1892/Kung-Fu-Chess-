#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ix {
class WebSocketServer;
class WebSocket;
}  // namespace ix

namespace kungfu::net {

// The one place allowed to touch ix:: directly (mirrors Img for OpenCV, SoundPlayer for
// winmm). Every connection is identified by a small opaque string id; callers never see
// an ix::WebSocket. Thread-safety note: onMessage/onConnect/onDisconnect fire on
// IXWebSocket's own background I/O thread(s), not the caller's thread - anything they do
// that touches shared game state needs its own locking (see Server/main.cpp).
class WsServerTransport {
public:
    using ConnectionId = std::string;
    using ConnectHandler = std::function<void(const ConnectionId&)>;
    using MessageHandler = std::function<void(const ConnectionId&, const std::string&)>;
    using DisconnectHandler = std::function<void(const ConnectionId&)>;

    explicit WsServerTransport(int port);
    ~WsServerTransport();

    // Handlers must be set before start().
    void onConnect(ConnectHandler handler);
    void onMessage(MessageHandler handler);
    void onDisconnect(DisconnectHandler handler);

    // Starts listening and returns immediately - IXWebSocket runs its own I/O thread(s).
    void start();
    void stop();

    void send(const ConnectionId& id, const std::string& text);
    void broadcast(const std::string& text);
    void close(const ConnectionId& id);

private:
    int port_;
    std::unique_ptr<ix::WebSocketServer> server_;

    ConnectHandler onConnect_;
    MessageHandler onMessage_;
    DisconnectHandler onDisconnect_;

    std::mutex clientsMutex_;
    std::unordered_map<ConnectionId, ix::WebSocket*> clients_;
};

}  // namespace kungfu::net
