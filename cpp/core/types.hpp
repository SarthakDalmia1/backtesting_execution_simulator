#pragma once

#include <cstdint>
#include <string>
#include <limits>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <functional>
#include <memory>

namespace backtest {

// ============================================================================
// Constants
// ============================================================================
constexpr double EPSILON = 1e-10;
constexpr int64_t NANOSECONDS_PER_SECOND = 1'000'000'000LL;
constexpr int64_t NANOSECONDS_PER_MILLISECOND = 1'000'000LL;
constexpr int64_t NANOSECONDS_PER_MICROSECOND = 1'000LL;
constexpr size_t CACHE_LINE_SIZE = 64;
constexpr size_t MAX_SYMBOL_LENGTH = 16;

// Price is stored as fixed-point integer (price * 10^PRICE_DECIMALS)
constexpr int PRICE_DECIMALS = 6;
constexpr int64_t PRICE_MULTIPLIER = 1'000'000LL;

// Quantity decimals
constexpr int QTY_DECIMALS = 4;
constexpr int64_t QTY_MULTIPLIER = 10'000LL;

// ============================================================================
// Nanosecond Timestamp
// ============================================================================
using Timestamp = int64_t;  // Nanoseconds since epoch

inline Timestamp now_ns() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
}

Timestamp to_timestamp(int year, int month, int day,
                       int hour = 0, int minute = 0, int second = 0, int nano = 0);

// ============================================================================
// Fixed-Point Price Type (for zero-copy and deterministic arithmetic)
// ============================================================================
struct alignas(8) Price {
    int64_t raw;  // Price * PRICE_MULTIPLIER
    
    Price() : raw(0) {}
    explicit Price(int64_t r) : raw(r) {}
    
    static Price from_double(double d) {
        return Price(static_cast<int64_t>(d * PRICE_MULTIPLIER + 0.5));
    }
    
    double to_double() const {
        return static_cast<double>(raw) / PRICE_MULTIPLIER;
    }
    
    Price operator+(const Price& other) const { return Price(raw + other.raw); }
    Price operator-(const Price& other) const { return Price(raw - other.raw); }
    Price operator*(int64_t mult) const { return Price(raw * mult); }
    Price operator/(int64_t div) const { return Price(raw / div); }
    
    bool operator==(const Price& other) const { return raw == other.raw; }
    bool operator!=(const Price& other) const { return raw != other.raw; }
    bool operator<(const Price& other) const { return raw < other.raw; }
    bool operator<=(const Price& other) const { return raw <= other.raw; }
    bool operator>(const Price& other) const { return raw > other.raw; }
    bool operator>=(const Price& other) const { return raw >= other.raw; }
    
    static Price max() { return Price(std::numeric_limits<int64_t>::max()); }
    static Price min() { return Price(std::numeric_limits<int64_t>::min()); }
    static Price zero() { return Price(0); }
};

// ============================================================================
// Fixed-Point Quantity Type
// ============================================================================
struct alignas(8) Quantity {
    int64_t raw;  // Quantity * QTY_MULTIPLIER
    
    Quantity() : raw(0) {}
    explicit Quantity(int64_t r) : raw(r) {}
    
    static Quantity from_double(double d) {
        return Quantity(static_cast<int64_t>(d * QTY_MULTIPLIER + 0.5));
    }
    
    static Quantity from_int(int64_t i) {
        return Quantity(i * QTY_MULTIPLIER);
    }
    
    double to_double() const {
        return static_cast<double>(raw) / QTY_MULTIPLIER;
    }
    
    int64_t to_int() const {
        return raw / QTY_MULTIPLIER;
    }
    
    Quantity operator+(const Quantity& other) const { return Quantity(raw + other.raw); }
    Quantity operator-(const Quantity& other) const { return Quantity(raw - other.raw); }
    Quantity operator*(int64_t mult) const { return Quantity(raw * mult); }
    Quantity operator/(int64_t div) const { return Quantity(raw / div); }
    
    Quantity& operator+=(const Quantity& other) { raw += other.raw; return *this; }
    Quantity& operator-=(const Quantity& other) { raw -= other.raw; return *this; }
    
    bool operator==(const Quantity& other) const { return raw == other.raw; }
    bool operator!=(const Quantity& other) const { return raw != other.raw; }
    bool operator<(const Quantity& other) const { return raw < other.raw; }
    bool operator<=(const Quantity& other) const { return raw <= other.raw; }
    bool operator>(const Quantity& other) const { return raw > other.raw; }
    bool operator>=(const Quantity& other) const { return raw >= other.raw; }
    
    static Quantity zero() { return Quantity(0); }
    bool is_zero() const { return raw == 0; }
};

// ============================================================================
// Symbol Identifier (Fixed-size for cache efficiency)
// ============================================================================
struct alignas(CACHE_LINE_SIZE) Symbol {
    char data[MAX_SYMBOL_LENGTH];
    
    Symbol() { std::memset(data, 0, MAX_SYMBOL_LENGTH); }
    
    explicit Symbol(const char* str);
    
    explicit Symbol(const std::string& str) : Symbol(str.c_str()) {}
    
    std::string to_string() const { return std::string(data); }
    
    bool operator==(const Symbol& other) const {
        return std::memcmp(data, other.data, MAX_SYMBOL_LENGTH) == 0;
    }
    
    bool operator!=(const Symbol& other) const {
        return !(*this == other);
    }
    
    bool operator<(const Symbol& other) const {
        return std::memcmp(data, other.data, MAX_SYMBOL_LENGTH) < 0;
    }
};

// Hash function for Symbol
struct SymbolHash {
    size_t operator()(const Symbol& s) const;
};

// ============================================================================
// Order Side
// ============================================================================
enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

inline Side opposite(Side side) {
    return side == Side::Buy ? Side::Sell : Side::Buy;
}

// ============================================================================
// Order Type
// ============================================================================
enum class OrderType : uint8_t {
    Market = 0,
    Limit = 1,
    StopMarket = 2,
    StopLimit = 3,
    IOC = 4,      // Immediate or Cancel
    FOK = 5,      // Fill or Kill
    GTC = 6       // Good Till Cancel (same as Limit for backtesting)
};

// ============================================================================
// Order Status
// ============================================================================
enum class OrderStatus : uint8_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    Rejected = 4,
    Expired = 5
};

// ============================================================================
// Time in Force
// ============================================================================
enum class TimeInForce : uint8_t {
    Day = 0,      // Valid for trading day
    GTC = 1,      // Good till cancelled
    IOC = 2,      // Immediate or cancel
    FOK = 3,      // Fill or kill
    GTD = 4       // Good till date
};

// ============================================================================
// Event Type
// ============================================================================
enum class EventType : uint8_t {
    MarketData = 0,
    OrderSubmit = 1,
    OrderCancel = 2,
    OrderFill = 3,
    OrderReject = 4,
    Trade = 5,
    PositionUpdate = 6,
    EndOfDay = 7,
    SessionStart = 8,
    SessionEnd = 9
};

// ============================================================================
// Order ID
// ============================================================================
using OrderId = uint64_t;
constexpr OrderId INVALID_ORDER_ID = 0;

// ============================================================================
// Trade ID
// ============================================================================
using TradeId = uint64_t;

// ============================================================================
// Unique ID Generator (Thread-safe)
// ============================================================================
class IdGenerator {
public:
    static OrderId next_order_id() {
        static std::atomic<OrderId> counter{1};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
    
    static TradeId next_trade_id() {
        static std::atomic<TradeId> counter{1};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
    
    static void reset() {
        // For testing - reset counters
    }
};

// ============================================================================
// Book Level (Price Level in Order Book)
// ============================================================================
struct alignas(32) BookLevel {
    Price price;
    Quantity quantity;
    int32_t order_count;
    int32_t padding;  // For alignment
    
    BookLevel() : price(), quantity(), order_count(0), padding(0) {}
    BookLevel(Price p, Quantity q, int32_t count = 1) 
        : price(p), quantity(q), order_count(count), padding(0) {}
};

// ============================================================================
// Market Data Tick
// ============================================================================
struct alignas(CACHE_LINE_SIZE) Tick {
    Timestamp timestamp;
    Symbol symbol;
    Price bid_price;
    Price ask_price;
    Quantity bid_size;
    Quantity ask_size;
    Price last_price;
    Quantity last_size;
    uint64_t volume;
    
    Tick() : timestamp(0), volume(0) {}
    
    Price mid_price() const {
        return Price((bid_price.raw + ask_price.raw) / 2);
    }
    
    Price spread() const {
        return ask_price - bid_price;
    }
};

// ============================================================================
// Fill Information
// ============================================================================
struct alignas(32) Fill {
    OrderId order_id;
    TradeId trade_id;
    Timestamp timestamp;
    Price fill_price;
    Quantity fill_quantity;
    Side side;
    bool is_maker;
    
    Fill() : order_id(0), trade_id(0), timestamp(0), side(Side::Buy), is_maker(false) {}
};

// ============================================================================
// Transaction Cost Configuration
// ============================================================================
struct TransactionCostConfig {
    double maker_fee_bps = 0.0;     // Maker fee in basis points
    double taker_fee_bps = 1.0;     // Taker fee in basis points
    double fixed_cost = 0.0;        // Fixed cost per trade
    double min_cost = 0.0;          // Minimum cost per trade
    double stamp_duty_bps = 0.0;    // Stamp duty (for UK stocks, etc.)
    
    double calculate_cost(double notional, bool is_maker) const;
};

// ============================================================================
// Slippage Model Configuration
// ============================================================================
struct SlippageConfig {
    double base_slippage_bps = 0.0;      // Base slippage in basis points
    double volume_impact_bps = 0.0;       // Additional slippage per volume
    double volatility_factor = 0.0;       // Slippage multiplier based on volatility
    bool use_market_impact = true;        // Whether to model market impact
    
    Price calculate_slippage(Price base_price, Quantity qty,
                            double avg_volume, Side side) const;
};

// ============================================================================
// Latency Model Configuration  
// ============================================================================
struct LatencyConfig {
    int64_t order_submit_latency_ns = 1000;      // Order submission latency
    int64_t order_cancel_latency_ns = 1000;      // Order cancellation latency
    int64_t market_data_latency_ns = 500;        // Market data latency
    int64_t fill_report_latency_ns = 500;        // Fill report latency
    bool deterministic = true;                    // Use deterministic latencies
    double latency_stddev_factor = 0.1;          // Std dev as fraction of mean
};

// ============================================================================
// Backtest Configuration
// ============================================================================
struct BacktestConfig {
    Timestamp start_time = 0;
    Timestamp end_time = 0;
    double initial_capital = 1'000'000.0;
    TransactionCostConfig transaction_costs;
    SlippageConfig slippage;
    LatencyConfig latency;
    bool enable_short_selling = true;
    double max_position_size = 1'000'000.0;
    bool enable_margin = false;
    double margin_requirement = 0.5;
};

}  // namespace backtest
