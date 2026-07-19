#include "WsClientTransport.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

namespace kungfu::net {

WsClientTransport::WsClientTransport(const std::string& url) {
    ix::initNetSystem();
    ws_ = std::make_unique<ix::WebSocket>();
    ws_->setUrl(url);
    ws_->enableAutomaticReconnection();
}

WsClientTransport::~WsClientTransport() {
    stop();
    ix::uninitNetSystem();
}

void WsClientTransport::onMessage(MessageHandler handler) {
    onMessage_ = std::move(handler);
}

void WsClientTransport::onOpen(OpenHandler handler) {
    onOpen_ = std::move(handler);
}

void WsClientTransport::start() {
    ws_->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            if (onMessage_) {
                onMessage_(msg->str);
            }
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            if (onOpen_) {
                onOpen_();
            }
        }
    });
    ws_->start();
}

void WsClientTransport::stop() {
    if (ws_) {
        ws_->stop();
    }
}

void WsClientTransport::send(const std::string& text) {
    ws_->send(text);
}

}  // namespace kungfu::net
