#pragma once

#include "types.hpp"
#include <cmath>
#include <string>
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

inline Price round_to_tick(Price price, Price tick_size) {
    if (tick_size.raw == 0) return price;
    int64_t rounded = ((price.raw + tick_size.raw / 2) / tick_size.raw) * tick_size.raw;
    return Price(rounded);
}

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
inline double vwap(const std::vector<double>& prices, 
                   const std::vector<double>& volumes) {
    if (prices.empty() || prices.size() != volumes.size()) return 0.0;
    double pv_sum = 0.0;
    double v_sum = 0.0;
    for (size_t i = 0; i < prices.size(); ++i) {
        pv_sum += prices[i] * volumes[i];
        v_sum += volumes[i];
    }
    return v_sum > 0 ? pv_sum / v_sum : 0.0;
}

}  // namespace math

// ============================================================================
// String Utilities
// ============================================================================
namespace string_utils {

inline std::string format_price(Price price, int decimals = 4) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << price.to_double();
    return oss.str();
}

inline std::string format_quantity(Quantity qty, int decimals = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << qty.to_double();
    return oss.str();
}

inline std::string format_timestamp(Timestamp ts) {
    time_t seconds = ts / NANOSECONDS_PER_SECOND;
    int64_t nanos = ts % NANOSECONDS_PER_SECOND;
    std::tm* tm = std::localtime(&seconds);
    
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(9) << nanos;
    return oss.str();
}

inline std::string format_pnl(double pnl) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (pnl >= 0) oss << "+";
    oss << pnl;
    return oss.str();
}

inline std::string side_to_string(Side side) {
    return side == Side::Buy ? "BUY" : "SELL";
}

inline std::string order_type_to_string(OrderType type) {
    switch (type) {
        case OrderType::Market: return "MARKET";
        case OrderType::Limit: return "LIMIT";
        case OrderType::StopMarket: return "STOP_MARKET";
        case OrderType::StopLimit: return "STOP_LIMIT";
        case OrderType::IOC: return "IOC";
        case OrderType::FOK: return "FOK";
        case OrderType::GTC: return "GTC";
        default: return "UNKNOWN";
    }
}

inline std::string order_status_to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIAL";
        case OrderStatus::Filled: return "FILLED";
        case OrderStatus::Cancelled: return "CANCELLED";
        case OrderStatus::Rejected: return "REJECTED";
        case OrderStatus::Expired: return "EXPIRED";
        default: return "UNKNOWN";
    }
}

}  // namespace string_utils

// ============================================================================
// Time Utilities
// ============================================================================
namespace time_utils {

inline Timestamp parse_timestamp(const std::string& str, 
                                  const std::string& format = "%Y-%m-%d %H:%M:%S") {
    std::tm tm{};
    std::istringstream iss(str);
    iss >> std::get_time(&tm, format.c_str());
    auto time_t_val = std::mktime(&tm);
    return static_cast<Timestamp>(time_t_val) * NANOSECONDS_PER_SECOND;
}

inline bool is_market_hours(Timestamp ts, int open_hour = 9, int open_min = 30,
                            int close_hour = 16, int close_min = 0) {
    time_t seconds = ts / NANOSECONDS_PER_SECOND;
    std::tm* tm = std::localtime(&seconds);
    int minutes = tm->tm_hour * 60 + tm->tm_min;
    int open_minutes = open_hour * 60 + open_min;
    int close_minutes = close_hour * 60 + close_min;
    return minutes >= open_minutes && minutes < close_minutes;
}

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
    
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
        // Could log or accumulate timing here
    }
    
    int64_t elapsed_ns() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_).count();
    }

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

}  // namespace backtest
