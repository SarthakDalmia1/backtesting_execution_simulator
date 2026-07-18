#pragma once

#include "core/types.hpp"
#include "events/event.hpp"
#include <queue>
#include <vector>
#include <mutex>
#include <atomic>
#include <optional>
#include <array>

namespace backtest {

// ============================================================================
// Time-Ordered Event Queue (Priority Queue)
// ============================================================================
class EventQueue {
public:
    EventQueue() : event_count_(0) {}
    
    // Push event to queue
    void push(Event event) {
        events_.push(std::move(event));
        ++event_count_;
    }
    
    // Pop next event (by timestamp)
    std::optional<Event> pop();
    
    // Peek at next event without removing
    const Event* peek() const {
        if (events_.empty()) {
            return nullptr;
        }
        return &events_.top();
    }
    
    // Check if queue is empty
    bool empty() const { return events_.empty(); }
    
    // Get number of events in queue
    size_t size() const { return events_.size(); }
    
    // Get total events processed
    size_t total_events() const { return event_count_; }
    
    // Clear all events
    void clear();

    // Get next event timestamp (or max if empty)
    Timestamp next_timestamp() const;

private:
    std::priority_queue<Event, std::vector<Event>, EventComparator> events_;
    size_t event_count_;
};

// ============================================================================
// Lock-Free Event Queue (Single Producer Single Consumer)
// ============================================================================
template<size_t Capacity = 65536>
class LockFreeEventQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
public:
    LockFreeEventQueue() : head_(0), tail_(0) {
        for (size_t i = 0; i < Capacity; ++i) {
            sequence_[i].store(i, std::memory_order_relaxed);
        }
    }
    
    // Push event (returns false if full)
    bool try_push(Event event) {
        size_t pos = tail_.load(std::memory_order_relaxed);
        
        for (;;) {
            size_t seq = sequence_[pos & (Capacity - 1)].load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            
            if (diff == 0) {
                if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    buffer_[pos & (Capacity - 1)] = std::move(event);
                    sequence_[pos & (Capacity - 1)].store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;  // Full
            } else {
                pos = tail_.load(std::memory_order_relaxed);
            }
        }
    }
    
    // Pop event (returns nullopt if empty)
    std::optional<Event> try_pop() {
        size_t pos = head_.load(std::memory_order_relaxed);
        
        for (;;) {
            size_t seq = sequence_[pos & (Capacity - 1)].load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            
            if (diff == 0) {
                if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    Event event = std::move(buffer_[pos & (Capacity - 1)]);
                    sequence_[pos & (Capacity - 1)].store(pos + Capacity, std::memory_order_release);
                    return event;
                }
            } else if (diff < 0) {
                return std::nullopt;  // Empty
            } else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }
    
    bool empty() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return head == tail;
    }
    
    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return tail - head;
    }

private:
    std::array<Event, Capacity> buffer_;
    std::array<std::atomic<size_t>, Capacity> sequence_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
};

// ============================================================================
// Multi-Source Event Merger (for combining multiple tick sources)
// ============================================================================
class EventMerger {
public:
    void add_source(EventQueue* source) {
        sources_.push_back(source);
    }

    // Get next event across all sources
    std::optional<Event> pop_next();

    bool empty() const;

private:
    std::vector<EventQueue*> sources_;
};

}  // namespace backtest
