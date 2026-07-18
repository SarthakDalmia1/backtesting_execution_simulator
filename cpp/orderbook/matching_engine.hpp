#pragma once

#include "core/types.hpp"
#include "core/memory_pool.hpp"
#include "orderbook/order.hpp"
#include "orderbook/orderbook.hpp"
#include <vector>
#include <functional>
#include <unordered_map>

namespace backtest {

// ============================================================================
// Match Result
// ============================================================================
struct MatchResult {
    std::vector<Fill> fills;
    Quantity remaining_quantity;
    bool fully_filled;
    
    MatchResult() : remaining_quantity(), fully_filled(false) {}
};

// ============================================================================
// Matching Engine - Price-Time Priority
// ============================================================================
class MatchingEngine {
public:
    using FillCallback = std::function<void(const Fill&)>;
    using TradeCallback = std::function<void(const Symbol&, Price, Quantity, Timestamp)>;
    
    explicit MatchingEngine(MemoryPool<Order>* order_pool = nullptr)
        : order_pool_(order_pool), current_time_(0) {}
    
    // Set callbacks
    void set_fill_callback(FillCallback cb) { fill_callback_ = std::move(cb); }
    void set_trade_callback(TradeCallback cb) { trade_callback_ = std::move(cb); }
    
    // Set current simulation time
    void set_time(Timestamp ts) { current_time_ = ts; }
    
    // Get or create order book for symbol
    OrderBook& get_order_book(const Symbol& symbol);

    const OrderBook* find_order_book(const Symbol& symbol) const {
        auto it = order_books_.find(symbol);
        return it != order_books_.end() ? &it->second : nullptr;
    }

    // Submit a new order
    Order* submit_order(const OrderRequest& request);

    // Cancel an order
    bool cancel_order(OrderId order_id);

    // Find order
    Order* find_order(OrderId order_id) {
        auto it = all_orders_.find(order_id);
        return it != all_orders_.end() ? it->second : nullptr;
    }

    // Get all orders for a strategy
    std::vector<Order*> get_orders_by_strategy(const std::string& strategy_id);

    // Cancel all orders for a strategy
    int cancel_all_orders(const std::string& strategy_id);

private:
    Order* allocate_order();
    void deallocate_order(Order* order);

    // Match a market order (takes liquidity)
    void match_market_order(Order* order, OrderBook& book);

    // Match a limit order
    void match_limit_order(Order* order, OrderBook& book);

    // Match IOC (Immediate or Cancel)
    void match_ioc_order(Order* order, OrderBook& book);

    // Match FOK (Fill or Kill)
    void match_fok_order(Order* order, OrderBook& book);

    // Generic matching against one side of the book
    template<bool IsBidSide>
    void match_against_book(Order* aggressor, OrderBookSide<IsBidSide>& passive_side);

    MemoryPool<Order>* order_pool_;
    Timestamp current_time_;
    std::unordered_map<Symbol, OrderBook, SymbolHash> order_books_;
    std::unordered_map<OrderId, Order*> all_orders_;
    FillCallback fill_callback_;
    TradeCallback trade_callback_;
};

}  // namespace backtest
