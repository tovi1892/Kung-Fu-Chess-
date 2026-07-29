#include "Matchmaker.hpp"

#include <chrono>
#include <stdexcept>
#include <variant>

#include "Network/MatchmakerProtocol.hpp"

namespace kungfu {

namespace {

// Same value as Server/GameServer.cpp's kEloMatchRangeElo - this service replicates
// RoomManager::matchWaiter's exact matching rule, just backed by a shared Redis queue instead
// of one shard's in-memory vector.
constexpr int kEloMatchRangeElo = 100;

long long nowEpochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

Matchmaker::Matchmaker(int listenPort, int shardCount, int quickMatchTimeoutMs, std::shared_ptr<RedisWaitQueue> queue,
                       net::Logger& logger)
    : shardCount_(shardCount),
      quickMatchTimeoutMs_(quickMatchTimeoutMs),
      queue_(std::move(queue)),
      logger_(logger),
      downstream_(listenPort) {
    if (shardCount_ <= 0) {
        throw std::invalid_argument("Matchmaker requires shardCount >= 1");
    }
}

Matchmaker::~Matchmaker() {
    stopSweep_ = true;
    if (sweepThread_.joinable()) {
        sweepThread_.join();
    }
}

void Matchmaker::run() {
    downstream_.onMessage([this](const ConnectionId& id, const std::string& text) { handleMessage(id, text); });
    downstream_.onDisconnect([this](const ConnectionId& id) { handleDisconnect(id); });
    downstream_.start();

    sweepThread_ = std::thread([this] { sweepLoop(); });

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(3600));
    }
}

void Matchmaker::handleMessage(const ConnectionId& id, const std::string& text) {
    const auto decoded = net::mm::decode(text);
    if (const auto* wait = std::get_if<net::mm::WaitMessage>(&decoded)) {
        onWaitRequest(id, wait->username, wait->rating);
    }
    // Anything else (malformed/unexpected) is silently ignored - the Gateway only ever sends
    // WAIT on this connection, per Network/MatchmakerProtocol.hpp.
}

void Matchmaker::onWaitRequest(const ConnectionId& id, const std::string& username, int rating) {
    std::optional<QueuedWaiter> matched;
    std::string roomKey;
    int shardIndex = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        matched = queue_->matchWaiter(rating, kEloMatchRangeElo);
        if (matched.has_value()) {
            waitingListMembers_.erase(matched->connectionId);
            roomKey = queue_->allocateRoomKey();
            shardIndex = nextShardIndex_.fetch_add(1, std::memory_order_relaxed) % shardCount_;
        } else {
            const long long deadline = nowEpochMs() + quickMatchTimeoutMs_;
            const std::string member = queue_->addWaiter(QueuedWaiter{id, username, rating, deadline});
            waitingListMembers_[id] = member;
        }
    }  // lock released here - never call downstream_.send() while holding mutex_ (see
       // Matchmaker.hpp's comment on why)

    if (matched.has_value()) {
        const std::string msg = net::mm::encodeMatched(static_cast<std::size_t>(shardIndex), roomKey);
        downstream_.send(matched->connectionId, msg);
        downstream_.send(id, msg);
        logger_.log("matched " + matched->username + " vs " + username + " -> room " + roomKey + " on shard " +
                    std::to_string(shardIndex));
    } else {
        logger_.log(username + " searching for a quick-match opponent (rating " + std::to_string(rating) + ")");
    }
}

void Matchmaker::handleDisconnect(const ConnectionId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = waitingListMembers_.find(id);
    if (it != waitingListMembers_.end()) {
        queue_->removeWaiterByMember(it->second);
        waitingListMembers_.erase(it);
    }
}

void Matchmaker::sweepLoop() {
    while (!stopSweep_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        const auto expired = queue_->reapExpired(nowEpochMs());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& waiter : expired) {
                waitingListMembers_.erase(waiter.connectionId);
            }
        }  // lock released before sending, same discipline as onWaitRequest above
        for (const auto& waiter : expired) {
            downstream_.send(waiter.connectionId, net::mm::encodeNoOpponent());
            logger_.log(waiter.username + " quick-match search timed out - no opponent found");
        }
    }
}

}  // namespace kungfu
