#include "WsServerTransport.hpp"

#include <stdexcept>
#include <vector>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

namespace kungfu::net {

WsServerTransport::WsServerTransport(int port) : port_(port) {
    ix::initNetSystem();
    server_ = std::make_unique<ix::WebSocketServer>(port_, "0.0.0.0");
}

WsServerTransport::~WsServerTransport() {
    stop();
    ix::uninitNetSystem();
}

void WsServerTransport::onConnect(ConnectHandler handler) {
    onConnect_ = std::move(handler);
}

void WsServerTransport::onMessage(MessageHandler handler) {
    onMessage_ = std::move(handler);
}

void WsServerTransport::onDisconnect(DisconnectHandler handler) {
    onDisconnect_ = std::move(handler);
}

void WsServerTransport::start() {
    server_->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket,
               const ix::WebSocketMessagePtr& msg) {
            const std::string id = connectionState->getId();

            if (msg->type == ix::WebSocketMessageType::Open) {
                {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    clients_[id] = &webSocket;
                }
                if (onConnect_) {
                    onConnect_(id);
                }
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                if (onMessage_) {
                    onMessage_(id, msg->str);
                }
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    clients_.erase(id);
                }
                if (onDisconnect_) {
                    onDisconnect_(id);
                }
            }
        });

    if (!server_->listenAndStart()) {
        throw std::runtime_error("WsServerTransport: failed to listen on port " + std::to_string(port_));
    }
}

void WsServerTransport::stop() {
    if (server_) {
        server_->stop();
    }
}

// send/broadcast/close never call into ix::WebSocket while holding clientsMutex_: a
// write to a connection whose peer already closed can make IXWebSocket synchronously
// invoke this same transport's onClientMessageCallback (Close) on the calling thread
// before send()/close() returns - which would then try to re-lock clientsMutex_ on a
// thread that already holds it. Copying out the raw pointer(s) first and only calling
// send()/close() after releasing the lock avoids that self-deadlock. The pointers stay
// valid regardless: they reference the ix::WebSocket each connection's own dedicated
// thread is still running inside (see WsServerTransport::start()'s Open handler) until
// that thread's run() returns, which can't happen concurrently with this call.
void WsServerTransport::send(const ConnectionId& id, const std::string& text) {
    ix::WebSocket* ws = nullptr;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(id);
        if (it != clients_.end()) {
            ws = it->second;
        }
    }
    if (ws) {
        ws->send(text);
    }
}

void WsServerTransport::broadcast(const std::string& text) {
    std::vector<ix::WebSocket*> targets;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        targets.reserve(clients_.size());
        for (auto& [id, ws] : clients_) {
            if (ws) {
                targets.push_back(ws);
            }
        }
    }
    for (auto* ws : targets) {
        ws->send(text);
    }
}

void WsServerTransport::close(const ConnectionId& id) {
    ix::WebSocket* ws = nullptr;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(id);
        if (it != clients_.end()) {
            ws = it->second;
        }
    }
    if (ws) {
        ws->close();
    }
}

}  // namespace kungfu::net
