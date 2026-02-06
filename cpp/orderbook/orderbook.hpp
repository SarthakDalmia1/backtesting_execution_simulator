#pragma once

#include "core/types.hpp"
#include "core/memory_pool.hpp"
#include "orderbook/order.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <algorithm>

namespace backtest {

// ============================================================================
// Price Level - All orders at a single price
// ============================================================================
class PriceLevel {
public:
    explicit PriceLevel(Price price) 
        : price_(price), total_quantity_(), order_count_(0),
          head_(nullptr), tail_(nullptr) {}
    
    Price price() const { return price_; }
    Quantity total_quantity() const { return total_quantity_; }
    int order_count() const { return order_count_; }
    bool empty() const { return order_count_ == 0; }
    
    // Add order to this price level (FIFO)
    void add_order(Order* order) {
        order->prev_at_price = tail_;
        order->next_at_price = nullptr;
        
        if (tail_) {
            tail_->next_at_price = order;
        } else {
            head_ = order;
        }
        tail_ = order;
        
        total_quantity_ += order->remaining_quantity;
        ++order_count_;
    }
    
    // Remove order from this price level
    void remove_order(Order* order) {
        if (order->prev_at_price) {
            order->prev_at_price->next_at_price = order->next_at_price;
        } else {
            head_ = order->next_at_price;
        }
        
        if (order->next_at_price) {
            order->next_at_price->prev_at_price = order->prev_at_price;
        } else {
            tail_ = order->prev_at_price;
        }
        
        total_quantity_ -= order->remaining_quantity;
        --order_count_;
        
        order->prev_at_price = nullptr;
        order->next_at_price = nullptr;
    }
    
    // Get first order (for matching)
    Order* front() const { return head_; }
    
    // Update quantity after partial fill
    void update_quantity(Quantity delta) {
        total_quantity_ -= delta;
    }
    
    // Iterate over orders
    template<typename Func>
    void for_each_order(Func&& func) const {
        Order* current = head_;
        while (current) {
            func(current);
            current = current->next_at_price;
        }
    }

private:
    Price price_;
    Quantity total_quantity_;
    int order_count_;
    Order* head_;
    Order* tail_;
};

// ============================================================================
// Limit Order Book - One side (bids or asks)
// ============================================================================
template<bool IsBidSide>
class OrderBookSide {
public:
    using PriceLevelMap = std::map<Price, PriceLevel*, 
        std::conditional_t<IsBidSide, std::greater<Price>, std::less<Price>>>;
    
    OrderBookSide() = default;
    
    ~OrderBookSide() {
        for (auto& [price, level] : levels_) {
            delete level;
        }
    }
    
    // Add order to book
    void add_order(Order* order) {
        auto it = levels_.find(order->price);
        PriceLevel* level;
        
        if (it == levels_.end()) {
            level = new PriceLevel(order->price);
            levels_[order->price] = level;
        } else {
            level = it->second;
        }
        
        level->add_order(order);
    }
    
    // Remove order from book
    void remove_order(Order* order) {
        auto it = levels_.find(order->price);
        if (it == levels_.end()) return;
        
        PriceLevel* level = it->second;
        level->remove_order(order);
        
        if (level->empty()) {
            delete level;
            levels_.erase(it);
        }
    }
    
    // Get best price level
    PriceLevel* best_level() {
        if (levels_.empty()) return nullptr;
        return levels_.begin()->second;
    }
    
    const PriceLevel* best_level() const {
        if (levels_.empty()) return nullptr;
        return levels_.begin()->second;
    }
    
    // Get best price
    Price best_price() const {
        if (levels_.empty()) {
            return IsBidSide ? Price::zero() : Price::max();
        }
        return levels_.begin()->first;
    }
    
    // Get total quantity at best price
    Quantity best_quantity() const {
        auto* level = best_level();
        return level ? level->total_quantity() : Quantity::zero();
    }
    
    // Get depth (number of price levels)
    size_t depth() const { return levels_.size(); }
    
    // Check if empty
    bool empty() const { return levels_.empty(); }
    
    // Get top N levels
    std::vector<BookLevel> get_depth(size_t n) const {
        std::vector<BookLevel> result;
        result.reserve(std::min(n, levels_.size()));
        
        size_t count = 0;
        for (const auto& [price, level] : levels_) {
            if (count >= n) break;
            result.emplace_back(price, level->total_quantity(), level->order_count());
            ++count;
        }
        
        return result;
    }
    
    // Access to all levels
    const PriceLevelMap& levels() const { return levels_; }

private:
    PriceLevelMap levels_;
};

// ============================================================================
// Full Order Book (Both Sides)
// ============================================================================
class OrderBook {
public:
    explicit OrderBook(const Symbol& symbol) : symbol_(symbol) {}
    
    const Symbol& symbol() const { return symbol_; }
    
    // Access bid and ask sides
    OrderBookSide<true>& bids() { return bids_; }
    const OrderBookSide<true>& bids() const { return bids_; }
    
    OrderBookSide<false>& asks() { return asks_; }
    const OrderBookSide<false>& asks() const { return asks_; }
    
    // Add order to book
    void add_order(Order* order) {
        orders_[order->id] = order;
        
        if (order->side == Side::Buy) {
            bids_.add_order(order);
        } else {
            asks_.add_order(order);
        }
    }
    
    // Remove order from book
    void remove_order(OrderId id) {
        auto it = orders_.find(id);
        if (it == orders_.end()) return;
        
        Order* order = it->second;
        
        if (order->side == Side::Buy) {
            bids_.remove_order(order);
        } else {
            asks_.remove_order(order);
        }
        
        orders_.erase(it);
    }
    
    void remove_order(Order* order) {
        if (order) remove_order(order->id);
    }
    
    // Find order by ID
    Order* find_order(OrderId id) {
        auto it = orders_.find(id);
        return it != orders_.end() ? it->second : nullptr;
    }
    
    const Order* find_order(OrderId id) const {
        auto it = orders_.find(id);
        return it != orders_.end() ? it->second : nullptr;
    }
    
    // Best bid/ask
    Price best_bid() const { return bids_.best_price(); }
    Price best_ask() const { return asks_.best_price(); }
    
    Quantity best_bid_quantity() const { return bids_.best_quantity(); }
    Quantity best_ask_quantity() const { return asks_.best_quantity(); }
    
    // Spread
    Price spread() const {
        if (bids_.empty() || asks_.empty()) return Price::max();
        return asks_.best_price() - bids_.best_price();
    }
    
    // Mid price
    Price mid_price() const {
        if (bids_.empty() || asks_.empty()) return Price::zero();
        return Price((bids_.best_price().raw + asks_.best_price().raw) / 2);
    }
    
    // Micro price (volume-weighted mid)
    Price micro_price() const {
        if (bids_.empty() || asks_.empty()) return Price::zero();
        
        Quantity bid_qty = bids_.best_quantity();
        Quantity ask_qty = asks_.best_quantity();
        int64_t total = bid_qty.raw + ask_qty.raw;
        
        if (total == 0) return mid_price();
        
        // Micro price = (bid * ask_qty + ask * bid_qty) / (bid_qty + ask_qty)
        int64_t weighted = (bids_.best_price().raw * ask_qty.raw + 
                           asks_.best_price().raw * bid_qty.raw) / total;
        return Price(weighted);
    }
    
    // Get order book snapshot
    struct Snapshot {
        Symbol symbol;
        Timestamp timestamp;
        std::vector<BookLevel> bids;
        std::vector<BookLevel> asks;
    };
    
    Snapshot get_snapshot(size_t depth = 10) const {
        Snapshot snap;
        snap.symbol = symbol_;
        snap.timestamp = now_ns();
        snap.bids = bids_.get_depth(depth);
        snap.asks = asks_.get_depth(depth);
        return snap;
    }
    
    // Total order count
    size_t order_count() const { return orders_.size(); }
    
    // Clear all orders
    void clear() {
        orders_.clear();
        bids_ = OrderBookSide<true>();
        asks_ = OrderBookSide<false>();
    }

private:
    Symbol symbol_;
    OrderBookSide<true> bids_;   // Sorted high to low
    OrderBookSide<false> asks_;  // Sorted low to high
    std::unordered_map<OrderId, Order*> orders_;
};

}  // namespace backtest
