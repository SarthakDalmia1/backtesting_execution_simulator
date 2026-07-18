#pragma once

#include "core/types.hpp"
#include "events/event.hpp"
#include <vector>
#include <fstream>
#include <sstream>
#include <functional>
#include <memory>
#include <cmath>
#include <algorithm>

namespace backtest {

// ============================================================================
// Market Data Feed Interface
// ============================================================================
class MarketDataFeed {
public:
    virtual ~MarketDataFeed() = default;
    
    // Get next tick (returns false if no more data)
    virtual bool next_tick(Tick& tick) = 0;
    
    // Reset to beginning
    virtual void reset() = 0;
    
    // Check if has more data
    virtual bool has_data() const = 0;
    
    // Get total number of ticks (if known)
    virtual size_t total_ticks() const { return 0; }
};

// ============================================================================
// Vector-Based Data Feed (in-memory)
// ============================================================================
class VectorDataFeed : public MarketDataFeed {
public:
    VectorDataFeed() : current_index_(0) {}
    
    void add_tick(const Tick& tick) {
        ticks_.push_back(tick);
    }
    
    void add_ticks(const std::vector<Tick>& ticks) {
        ticks_.insert(ticks_.end(), ticks.begin(), ticks.end());
    }
    
    // Sort ticks by timestamp
    void sort_by_timestamp();

    bool next_tick(Tick& tick) override;
    
    void reset() override {
        current_index_ = 0;
    }
    
    bool has_data() const override {
        return current_index_ < ticks_.size();
    }
    
    size_t total_ticks() const override {
        return ticks_.size();
    }
    
    const std::vector<Tick>& ticks() const { return ticks_; }

private:
    std::vector<Tick> ticks_;
    size_t current_index_;
};

// ============================================================================
// Synthetic Tick Generator (for testing)
// ============================================================================
class SyntheticTickGenerator : public MarketDataFeed {
public:
    SyntheticTickGenerator(
        const Symbol& symbol,
        double initial_price,
        double volatility,
        Timestamp start_time,
        Timestamp end_time,
        int64_t tick_interval_ns = NANOSECONDS_PER_MILLISECOND * 100  // 100ms default
    ) : symbol_(symbol)
      , current_price_(initial_price)
      , volatility_(volatility)
      , current_time_(start_time)
      , start_time_(start_time)
      , end_time_(end_time)
      , tick_interval_ns_(tick_interval_ns)
      , ticks_generated_(0) {
        // Simple random seed
        seed_ = static_cast<uint64_t>(start_time);
    }

    bool next_tick(Tick& tick) override;

    void reset() override;
    
    bool has_data() const override {
        return current_time_ < end_time_;
    }
    
    size_t total_ticks() const override {
        return static_cast<size_t>((end_time_ - start_time_) / tick_interval_ns_);
    }

private:
    // Simple xorshift random number generator
    double generate_random();

    
    Symbol symbol_;
    double current_price_;
    double volatility_;
    Timestamp current_time_;
    Timestamp start_time_;
    Timestamp end_time_;
    int64_t tick_interval_ns_;
    uint64_t seed_;
    size_t ticks_generated_;
};

// ============================================================================
// Multi-Symbol Data Feed Aggregator
// ============================================================================
class MultiSymbolFeed : public MarketDataFeed {
public:
    void add_feed(std::unique_ptr<MarketDataFeed> feed) {
        feeds_.push_back(std::move(feed));
    }
    
    bool next_tick(Tick& tick) override;

    void reset() override;

    bool has_data() const override;

private:
    std::vector<std::unique_ptr<MarketDataFeed>> feeds_;
    int current_feed_idx_ = -1;
};

}  // namespace backtest
