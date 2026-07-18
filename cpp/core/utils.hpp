#pragma once

#include "types.hpp"
#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace backtest {

// ============================================================================
// Mathematical Utilities
// ============================================================================
namespace math {

inline double clamp(double value, double min_val, double max_val) {
    return std::max(min_val, std::min(max_val, value));
}

inline bool approximately_equal(double a, double b, double epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

inline double round_to_tick(double price, double tick_size) {
    return std::round(price / tick_size) * tick_size;
}

Price round_to_tick(Price price, Price tick_size);

// Simple moving average
template<typename Container>
double sma(const Container& prices, size_t period) {
    if (prices.size() < period) return 0.0;
    double sum = 0.0;
    auto it = prices.end() - period;
    for (size_t i = 0; i < period; ++i, ++it) {
        sum += *it;
    }
    return sum / period;
}

// Exponential moving average
inline double ema(double current_ema, double new_value, double alpha) {
    return alpha * new_value + (1.0 - alpha) * current_ema;
}

// Standard deviation
template<typename Container>
double stddev(const Container& values) {
    if (values.size() < 2) return 0.0;
    double mean = 0.0;
    for (const auto& v : values) mean += v;
    mean /= values.size();
    
    double sq_sum = 0.0;
    for (const auto& v : values) {
        double diff = v - mean;
        sq_sum += diff * diff;
    }
    return std::sqrt(sq_sum / (values.size() - 1));
}

// VWAP calculation
double vwap(const std::vector<double>& prices,
            const std::vector<double>& volumes);

}  // namespace math

// ============================================================================
// String Utilities
// ============================================================================
namespace string_utils {

std::string format_price(Price price, int decimals = 4);

std::string format_quantity(Quantity qty, int decimals = 2);

std::string format_timestamp(Timestamp ts);

std::string format_pnl(double pnl);

inline std::string side_to_string(Side side) {
    return side == Side::Buy ? "BUY" : "SELL";
}

std::string order_type_to_string(OrderType type);

std::string order_status_to_string(OrderStatus status);

}  // namespace string_utils

// ============================================================================
// Time Utilities
// ============================================================================
namespace time_utils {

Timestamp parse_timestamp(const std::string& str,
                          const std::string& format = "%Y-%m-%d %H:%M:%S");

bool is_market_hours(Timestamp ts, int open_hour = 9, int open_min = 30,
                     int close_hour = 16, int close_min = 0);

inline Timestamp add_nanoseconds(Timestamp ts, int64_t nanos) {
    return ts + nanos;
}

inline Timestamp add_microseconds(Timestamp ts, int64_t micros) {
    return ts + micros * NANOSECONDS_PER_MICROSECOND;
}

inline Timestamp add_milliseconds(Timestamp ts, int64_t millis) {
    return ts + millis * NANOSECONDS_PER_MILLISECOND;
}

inline Timestamp add_seconds(Timestamp ts, int64_t secs) {
    return ts + secs * NANOSECONDS_PER_SECOND;
}

inline int64_t diff_nanoseconds(Timestamp end, Timestamp start) {
    return end - start;
}

inline double diff_seconds(Timestamp end, Timestamp start) {
    return static_cast<double>(end - start) / NANOSECONDS_PER_SECOND;
}

}  // namespace time_utils

// ============================================================================
// Benchmark Timer
// ============================================================================
class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& name)
        : name_(name), start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer();

    int64_t elapsed_ns() const;

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

}  // namespace backtest
