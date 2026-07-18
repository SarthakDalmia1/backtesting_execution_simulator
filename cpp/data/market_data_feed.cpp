#include "market_data_feed.hpp"

#include <limits>

namespace backtest {

void VectorDataFeed::sort_by_timestamp() {
    std::sort(ticks_.begin(), ticks_.end(),
              [](const Tick& a, const Tick& b) {
                  return a.timestamp < b.timestamp;
              });
}

bool VectorDataFeed::next_tick(Tick& tick) {
    if (current_index_ >= ticks_.size()) {
        return false;
    }
    tick = ticks_[current_index_++];
    return true;
}

bool SyntheticTickGenerator::next_tick(Tick& tick) {
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

void SyntheticTickGenerator::reset() {
    current_time_ = start_time_;
    seed_ = static_cast<uint64_t>(start_time_);
    ticks_generated_ = 0;
}

double SyntheticTickGenerator::generate_random() {
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;

    // Convert to [-1, 1] range
    double normalized = static_cast<double>(seed_) /
                       static_cast<double>(std::numeric_limits<uint64_t>::max());
    return (normalized - 0.5) * 2.0;
}

bool MultiSymbolFeed::next_tick(Tick& tick) {
    // Ideally we would pick the feed with the earliest next timestamp, but that
    // needs per-feed peek/buffering. For now we assume feeds are individually
    // sorted and cycle through them round-robin.
    while (!feeds_.empty()) {
        current_feed_idx_ = (current_feed_idx_ + 1) % feeds_.size();
        if (feeds_[current_feed_idx_]->next_tick(tick)) {
            return true;
        }
    }

    return false;
}

void MultiSymbolFeed::reset() {
    for (auto& feed : feeds_) {
        feed->reset();
    }
    current_feed_idx_ = -1;
}

bool MultiSymbolFeed::has_data() const {
    for (const auto& feed : feeds_) {
        if (feed->has_data()) return true;
    }
    return false;
}

}  // namespace backtest
