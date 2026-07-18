#pragma once

#include "types.hpp"
#include <cstdint>

namespace backtest {

// ============================================================================
// High-Resolution Timestamp Utilities
// ============================================================================
class HighResTimestamp {
public:
    // Get current timestamp in nanoseconds
    static Timestamp now() {
        return now_ns();
    }
    
    // Convert to components
    struct Components {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        int millisecond;
        int microsecond;
        int nanosecond;
    };
    
    static Components decompose(Timestamp ts);

    // Create timestamp from components
    static Timestamp from_components(const Components& c);
    
    // Round down to nearest second
    static Timestamp floor_second(Timestamp ts) {
        return (ts / NANOSECONDS_PER_SECOND) * NANOSECONDS_PER_SECOND;
    }
    
    // Round down to nearest millisecond
    static Timestamp floor_millisecond(Timestamp ts) {
        return (ts / NANOSECONDS_PER_MILLISECOND) * NANOSECONDS_PER_MILLISECOND;
    }
    
    // Round down to nearest microsecond
    static Timestamp floor_microsecond(Timestamp ts) {
        return (ts / NANOSECONDS_PER_MICROSECOND) * NANOSECONDS_PER_MICROSECOND;
    }
};

// ============================================================================
// Deterministic Timestamp Generator (for replay)
// ============================================================================
class SimulatedClock {
public:
    SimulatedClock() : current_time_(0) {}
    explicit SimulatedClock(Timestamp start) : current_time_(start) {}
    
    Timestamp now() const { return current_time_; }
    
    void set_time(Timestamp ts) { current_time_ = ts; }
    
    void advance(int64_t nanoseconds) {
        current_time_ += nanoseconds;
    }
    
    void advance_microseconds(int64_t micros) {
        current_time_ += micros * NANOSECONDS_PER_MICROSECOND;
    }
    
    void advance_milliseconds(int64_t millis) {
        current_time_ += millis * NANOSECONDS_PER_MILLISECOND;
    }
    
    void advance_seconds(int64_t secs) {
        current_time_ += secs * NANOSECONDS_PER_SECOND;
    }
    
private:
    Timestamp current_time_;
};

}  // namespace backtest
