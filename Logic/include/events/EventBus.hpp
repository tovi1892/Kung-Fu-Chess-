#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace kungfu {

// A minimal synchronous pub/sub channel for one event type. publish() calls every
// subscribed handler immediately, in-process, on the caller's thread - matching how the
// whole app already runs (one thread, ticked from main.cpp's loop). Reused as-is for
// every event type in GameEvents.hpp rather than one bespoke observer list per event.
template <typename EventT>
class EventBus {
public:
    using Handler = std::function<void(const EventT&)>;

    // Returns an id that can later be passed to unsubscribe().
    size_t subscribe(Handler handler) {
        const size_t id = nextId_++;
        handlers_.emplace_back(id, std::move(handler));
        return id;
    }

    void unsubscribe(size_t id) {
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if (it->first == id) {
                handlers_.erase(it);
                return;
            }
        }
    }

    void publish(const EventT& event) const {
        for (const auto& [id, handler] : handlers_) {
            if (handler) {
                handler(event);
            }
        }
    }

private:
    std::vector<std::pair<size_t, Handler>> handlers_;
    size_t nextId_ = 0;
};

}  // namespace kungfu
