#include "event_queue.hpp"

#include <limits>

namespace backtest {

std::optional<Event> EventQueue::pop() {
    if (events_.empty()) {
        return std::nullopt;
    }

    Event event = std::move(const_cast<Event&>(events_.top()));
    events_.pop();
    return event;
}

void EventQueue::clear() {
    while (!events_.empty()) {
        events_.pop();
    }
}

Timestamp EventQueue::next_timestamp() const {
    if (events_.empty()) {
        return std::numeric_limits<Timestamp>::max();
    }
    return get_event_timestamp(events_.top());
}

std::optional<Event> EventMerger::pop_next() {
    EventQueue* best_source = nullptr;
    Timestamp best_time = std::numeric_limits<Timestamp>::max();

    for (auto* source : sources_) {
        if (!source->empty()) {
            Timestamp ts = source->next_timestamp();
            if (ts < best_time) {
                best_time = ts;
                best_source = source;
            }
        }
    }

    if (best_source) {
        return best_source->pop();
    }

    return std::nullopt;
}

bool EventMerger::empty() const {
    for (auto* source : sources_) {
        if (!source->empty()) return false;
    }
    return true;
}

}  // namespace backtest
