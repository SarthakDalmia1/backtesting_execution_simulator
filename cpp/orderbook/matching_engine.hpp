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
    OrderBook& get_order_book(const Symbol& symbol) {
        auto it = order_books_.find(symbol);
        if (it == order_books_.end()) {
            auto [new_it, inserted] = order_books_.emplace(symbol, OrderBook(symbol));
            return new_it->second;
        }
        return it->second;
    }
    
    const OrderBook* find_order_book(const Symbol& symbol) const {
        auto it = order_books_.find(symbol);
        return it != order_books_.end() ? &it->second : nullptr;
    }
    
    // Submit a new order
    Order* submit_order(const OrderRequest& request) {
        Order* order = allocate_order();
        
        order->id = IdGenerator::next_order_id();
        order->symbol = request.symbol;
        order->side = request.side;
        order->type = request.type;
        order->tif = request.tif;
        order->price = request.price;
        order->stop_price = request.stop_price;
        order->quantity = request.quantity;
        order->remaining_quantity = request.quantity;
        order->client_order_id = request.client_order_id;
        order->strategy_id = request.strategy_id;
        order->submit_time = current_time_;
        order->last_update_time = current_time_;
        order->status = OrderStatus::New;
        
        // Track order
        all_orders_[order->id] = order;
        
        // Process based on order type
        OrderBook& book = get_order_book(request.symbol);
        
        if (order->type == OrderType::Market) {
            match_market_order(order, book);
        } else if (order->type == OrderType::Limit || order->type == OrderType::GTC) {
            match_limit_order(order, book);
        } else if (order->type == OrderType::IOC) {
            match_ioc_order(order, book);
        } else if (order->type == OrderType::FOK) {
            match_fok_order(order, book);
        }
        
        return order;
    }
    
    // Cancel an order
    bool cancel_order(OrderId order_id) {
        auto it = all_orders_.find(order_id);
        if (it == all_orders_.end()) return false;
        
        Order* order = it->second;
        if (!order->is_active()) return false;
        
        OrderBook& book = get_order_book(order->symbol);
        book.remove_order(order);
        order->cancel();
        
        return true;
    }
    
    // Find order
    Order* find_order(OrderId order_id) {
        auto it = all_orders_.find(order_id);
        return it != all_orders_.end() ? it->second : nullptr;
    }
    
    // Get all orders for a strategy
    std::vector<Order*> get_orders_by_strategy(const std::string& strategy_id) {
        std::vector<Order*> result;
        for (auto& [id, order] : all_orders_) {
            if (order->strategy_id == strategy_id) {
                result.push_back(order);
            }
        }
        return result;
    }
    
    // Cancel all orders for a strategy
    int cancel_all_orders(const std::string& strategy_id) {
        int count = 0;
        for (auto& [id, order] : all_orders_) {
            if (order->strategy_id == strategy_id && order->is_active()) {
                OrderBook& book = get_order_book(order->symbol);
                book.remove_order(order);
                order->cancel();
                ++count;
            }
        }
        return count;
    }

private:
    Order* allocate_order() {
        if (order_pool_) {
            return order_pool_->construct();
        }
        return new Order();
    }
    
    void deallocate_order(Order* order) {
        if (order_pool_) {
            order_pool_->destroy(order);
        } else {
            delete order;
        }
    }
    
    // Match a market order (takes liquidity)
    void match_market_order(Order* order, OrderBook& book) {
        if (order->side == Side::Buy) {
            match_against_book(order, book.asks());
        } else {
            match_against_book(order, book.bids());
        }
        
        // Market order that couldn't fill completely gets cancelled
        if (order->is_active()) {
            order->cancel();
        }
    }
    
    // Match a limit order
    void match_limit_order(Order* order, OrderBook& book) {
        // First try to match against opposite side
        if (order->side == Side::Buy) {
            // Buy order matches against asks
            if (!book.asks().empty() && order->price >= book.asks().best_price()) {
                match_against_book(order, book.asks());
            }
        } else {
            // Sell order matches against bids
            if (!book.bids().empty() && order->price <= book.bids().best_price()) {
                match_against_book(order, book.bids());
            }
        }
        
        // If order still has quantity, add to book
        if (order->is_active() && !order->remaining_quantity.is_zero()) {
            book.add_order(order);
        }
    }
    
    // Match IOC (Immediate or Cancel)
    void match_ioc_order(Order* order, OrderBook& book) {
        if (order->side == Side::Buy) {
            match_against_book(order, book.asks());
        } else {
            match_against_book(order, book.bids());
        }
        
        // Cancel any remaining quantity
        if (order->is_active()) {
            order->cancel();
        }
    }
    
    // Match FOK (Fill or Kill)
    void match_fok_order(Order* order, OrderBook& book) {
        // Check if we can fill the entire order
        Quantity available = Quantity::zero();
        
        if (order->side == Side::Buy) {
            for (const auto& [price, level] : book.asks().levels()) {
                if (price > order->price) break;
                available += level->total_quantity();
                if (available >= order->quantity) break;
            }
        } else {
            for (const auto& [price, level] : book.bids().levels()) {
                if (price < order->price) break;
                available += level->total_quantity();
                if (available >= order->quantity) break;
            }
        }
        
        if (available >= order->quantity) {
            // Can fill completely
            if (order->side == Side::Buy) {
                match_against_book(order, book.asks());
            } else {
                match_against_book(order, book.bids());
            }
        } else {
            // Cannot fill, reject
            order->reject("Insufficient liquidity for FOK order");
        }
    }
    
    // Generic matching against one side of the book
    template<bool IsBidSide>
    void match_against_book(Order* aggressor, OrderBookSide<IsBidSide>& passive_side) {
        while (!aggressor->remaining_quantity.is_zero() && !passive_side.empty()) {
            PriceLevel* level = passive_side.best_level();
            if (!level) break;
            
            // Check price compatibility
            if constexpr (IsBidSide) {
                // Aggressor is selling, passive is buying
                if (aggressor->type == OrderType::Limit && 
                    aggressor->price > level->price()) {
                    break;
                }
            } else {
                // Aggressor is buying, passive is selling
                if (aggressor->type == OrderType::Limit && 
                    aggressor->price < level->price()) {
                    break;
                }
            }
            
            // Match against orders at this level
            while (!aggressor->remaining_quantity.is_zero() && !level->empty()) {
                Order* passive = level->front();
                
                Quantity fill_qty = std::min(aggressor->remaining_quantity, 
                                            passive->remaining_quantity);
                Price fill_price = passive->price;  // Price-time priority
                
                // Create fill for aggressor
                Fill aggressor_fill;
                aggressor_fill.order_id = aggressor->id;
                aggressor_fill.trade_id = IdGenerator::next_trade_id();
                aggressor_fill.timestamp = current_time_;
                aggressor_fill.fill_price = fill_price;
                aggressor_fill.fill_quantity = fill_qty;
                aggressor_fill.side = aggressor->side;
                aggressor_fill.is_maker = false;
                
                // Create fill for passive
                Fill passive_fill;
                passive_fill.order_id = passive->id;
                passive_fill.trade_id = aggressor_fill.trade_id;
                passive_fill.timestamp = current_time_;
                passive_fill.fill_price = fill_price;
                passive_fill.fill_quantity = fill_qty;
                passive_fill.side = passive->side;
                passive_fill.is_maker = true;
                
                // Apply fills
                aggressor->apply_fill(fill_qty, fill_price);
                passive->apply_fill(fill_qty, fill_price);
                level->update_quantity(fill_qty);
                
                // Notify callbacks
                if (fill_callback_) {
                    fill_callback_(aggressor_fill);
                    fill_callback_(passive_fill);
                }
                
                if (trade_callback_) {
                    trade_callback_(aggressor->symbol, fill_price, fill_qty, current_time_);
                }
                
                // Remove passive order if filled
                if (passive->remaining_quantity.is_zero()) {
                    // Get book reference before removing
                    OrderBook& book = get_order_book(passive->symbol);
                    // remove_order handles both level and map cleanup
                    book.remove_order(passive);
                }
            }
            
            // Remove empty level
            if (level->empty()) {
                // Level will be removed by OrderBookSide automatically
            }
        }
    }
    
    MemoryPool<Order>* order_pool_;
    Timestamp current_time_;
    std::unordered_map<Symbol, OrderBook, SymbolHash> order_books_;
    std::unordered_map<OrderId, Order*> all_orders_;
    FillCallback fill_callback_;
    TradeCallback trade_callback_;
};

}  // namespace backtest
