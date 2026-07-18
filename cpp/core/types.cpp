#include "types.hpp"

#include <algorithm>
#include <ctime>

namespace backtest {

Timestamp to_timestamp(int year, int month, int day,
                       int hour, int minute, int second, int nano) {
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    auto time_t_val = std::mktime(&tm);
    return static_cast<Timestamp>(time_t_val) * NANOSECONDS_PER_SECOND + nano;
}

Symbol::Symbol(const char* str) {
    std::memset(data, 0, MAX_SYMBOL_LENGTH);
    std::strncpy(data, str, MAX_SYMBOL_LENGTH - 1);
}

size_t SymbolHash::operator()(const Symbol& s) const {
    // FNV-1a hash
    size_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < MAX_SYMBOL_LENGTH && s.data[i] != '\0'; ++i) {
        hash ^= static_cast<size_t>(s.data[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

double TransactionCostConfig::calculate_cost(double notional, bool is_maker) const {
    double fee_bps = is_maker ? maker_fee_bps : taker_fee_bps;
    double cost = notional * fee_bps / 10000.0 + fixed_cost;
    cost += notional * stamp_duty_bps / 10000.0;
    return std::max(cost, min_cost);
}

Price SlippageConfig::calculate_slippage(Price base_price, Quantity qty,
                                         double avg_volume, Side side) const {
    double slippage_bps = base_slippage_bps;

    if (avg_volume > 0 && volume_impact_bps > 0) {
        double participation_rate = qty.to_double() / avg_volume;
        slippage_bps += volume_impact_bps * participation_rate;
    }

    int64_t slippage_raw = static_cast<int64_t>(
        base_price.raw * slippage_bps / 10000.0);

    return side == Side::Buy ? Price(slippage_raw) : Price(-slippage_raw);
}

}  // namespace backtest
