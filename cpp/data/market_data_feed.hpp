#pragma once

#include "core/types.hpp"
#include "events/event.hpp"
#include <vector>
#include <fstream>
#include <sstream>
#include <functional>
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
    void sort_by_timestamp() {
        std::sort(ticks_.begin(), ticks_.end(),
                  [](const Tick& a, const Tick& b) {
                      return a.timestamp < b.timestamp;
                  });
    }
    
    bool next_tick(Tick& tick) override {
        if (current_index_ >= ticks_.size()) {
            return false;
        }
        tick = ticks_[current_index_++];
        return true;
    }
    
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
    
    bool next_tick(Tick& tick) override {
        if (current_time_ >= end_time_) {
            return false;
        }
        
        // Generate price movement
        double random = generate_random();
        double price_change = current_price_ * volatility_ * random * 
                              std::sqrt(static_cast<double>(tick_interval_ns_) / 
                                       NANOSECONDS_PER_SECOND / 86400.0);
        current_price_ += price_change;
        current_price_ = std::max(current_price_, 0.01);  // Floor at 0.01
        
        // Generate spread (typically 0.01% to 0.1%)
        double spread_pct = 0.0005 + 0.0005 * std::abs(random);
        double half_spread = current_price_ * spread_pct / 2.0;
        
        tick.timestamp = current_time_;
        tick.symbol = symbol_;
        tick.bid_price = Price::from_double(current_price_ - half_spread);
        tick.ask_price = Price::from_double(current_price_ + half_spread);
        tick.bid_size = Quantity::from_int(100 + static_cast<int>(std::abs(random) * 900));
        tick.ask_size = Quantity::from_int(100 + static_cast<int>(std::abs(random) * 900));
        tick.last_price = Price::from_double(current_price_);
        tick.last_size = Quantity::from_int(50 + static_cast<int>(std::abs(random) * 200));
        tick.volume = static_cast<uint64_t>(1000000 * (1.0 + random));
        
        current_time_ += tick_interval_ns_;
        ++ticks_generated_;
        
        return true;
    }
    
    void reset() override {
        current_time_ = start_time_;
        seed_ = static_cast<uint64_t>(start_time_);
        ticks_generated_ = 0;
    }
    
    bool has_data() const override {
        return current_time_ < end_time_;
    }
    
    size_t total_ticks() const override {
        return static_cast<size_t>((end_time_ - start_time_) / tick_interval_ns_);
    }

private:
    // Simple xorshift random number generator
    double generate_random() {
        seed_ ^= seed_ << 13;
        seed_ ^= seed_ >> 17;
        seed_ ^= seed_ << 5;
        
        // Convert to [-1, 1] range
        double normalized = static_cast<double>(seed_) / 
                           static_cast<double>(std::numeric_limits<uint64_t>::max());
        return (normalized - 0.5) * 2.0;
    }
    
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
    
    bool next_tick(Tick& tick) override {
        // Find feed with earliest timestamp
        Tick best_tick;
        int best_feed_idx = -1;
        Timestamp best_time = std::numeric_limits<Timestamp>::max();
        
        for (size_t i = 0; i < feeds_.size(); ++i) {
            if (!feeds_[i]->has_data()) continue;
            
            Tick candidate;
            // Peek at next tick (this is a limitation - would need buffering)
            // For now, we'll assume feeds are sorted and process round-robin
        }
        
        // Simple implementation: just cycle through feeds
        while (!feeds_.empty()) {
            current_feed_idx_ = (current_feed_idx_ + 1) % feeds_.size();
            if (feeds_[current_feed_idx_]->next_tick(tick)) {
                return true;
            }
        }
        
        return false;
    }
    
    void reset() override {
        for (auto& feed : feeds_) {
            feed->reset();
        }
        current_feed_idx_ = -1;
    }
    
    bool has_data() const override {
        for (const auto& feed : feeds_) {
            if (feed->has_data()) return true;
        }
        return false;
    }

private:
    std::vector<std::unique_ptr<MarketDataFeed>> feeds_;
    int current_feed_idx_ = -1;
};

}  // namespace backtest
