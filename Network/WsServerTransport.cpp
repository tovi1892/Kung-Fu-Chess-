#include "WsServerTransport.hpp"

#include <stdexcept>

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

void WsServerTransport::send(const ConnectionId& id, const std::string& text) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clients_.find(id);
    if (it != clients_.end() && it->second) {
        it->second->send(text);
    }
}

void WsServerTransport::broadcast(const std::string& text) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& [id, ws] : clients_) {
        if (ws) {
            ws->send(text);
        }
    }
}

void WsServerTransport::close(const ConnectionId& id) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clients_.find(id);
    if (it != clients_.end() && it->second) {
        it->second->close();
    }
}

}  // namespace kungfu::net
