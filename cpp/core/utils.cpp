#include "utils.hpp"

#include <ctime>

namespace backtest {

namespace math {

Price round_to_tick(Price price, Price tick_size) {
    if (tick_size.raw == 0) return price;
    int64_t rounded = ((price.raw + tick_size.raw / 2) / tick_size.raw) * tick_size.raw;
    return Price(rounded);
}

double vwap(const std::vector<double>& prices,
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

namespace string_utils {

std::string format_price(Price price, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << price.to_double();
    return oss.str();
}

std::string format_quantity(Quantity qty, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << qty.to_double();
    return oss.str();
}

std::string format_timestamp(Timestamp ts) {
    time_t seconds = ts / NANOSECONDS_PER_SECOND;
    int64_t nanos = ts % NANOSECONDS_PER_SECOND;
    std::tm* tm = std::localtime(&seconds);

    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(9) << nanos;
    return oss.str();
}

std::string format_pnl(double pnl) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (pnl >= 0) oss << "+";
    oss << pnl;
    return oss.str();
}

std::string order_type_to_string(OrderType type) {
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

std::string order_status_to_string(OrderStatus status) {
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

namespace time_utils {

Timestamp parse_timestamp(const std::string& str, const std::string& format) {
    std::tm tm{};
    std::istringstream iss(str);
    iss >> std::get_time(&tm, format.c_str());
    auto time_t_val = std::mktime(&tm);
    return static_cast<Timestamp>(time_t_val) * NANOSECONDS_PER_SECOND;
}

bool is_market_hours(Timestamp ts, int open_hour, int open_min,
                     int close_hour, int close_min) {
    time_t seconds = ts / NANOSECONDS_PER_SECOND;
    std::tm* tm = std::localtime(&seconds);
    int minutes = tm->tm_hour * 60 + tm->tm_min;
    int open_minutes = open_hour * 60 + open_min;
    int close_minutes = close_hour * 60 + close_min;
    return minutes >= open_minutes && minutes < close_minutes;
}

}  // namespace time_utils

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
    (void)duration;  // Could log or accumulate timing here
}

int64_t ScopedTimer::elapsed_ns() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_).count();
}

}  // namespace backtest
