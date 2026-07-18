#pragma once

#include "core/types.hpp"
#include <string>

namespace backtest {

// ============================================================================
// Order Structure - Cache-aligned for performance
// ============================================================================
struct alignas(CACHE_LINE_SIZE) Order {
    OrderId id;                     // Unique order ID
    Symbol symbol;                  // Trading symbol
    Side side;                      // Buy or Sell
    OrderType type;                 // Market, Limit, etc.
    TimeInForce tif;                // Time in force
    OrderStatus status;             // Current status
    
    Price price;                    // Limit price (0 for market orders)
    Price stop_price;               // Stop trigger price
    Quantity quantity;              // Original quantity
    Quantity filled_quantity;       // Quantity filled
    Quantity remaining_quantity;    // Quantity remaining
    
    Timestamp submit_time;          // When order was submitted
    Timestamp last_update_time;     // Last status update time
    Timestamp expiry_time;          // When order expires (0 = no expiry)
    
    double avg_fill_price;          // Average fill price
    std::string client_order_id;    // Client-assigned ID
    std::string strategy_id;        // Strategy that submitted this order
    
    // Linked list pointers for order book
    Order* next_at_price;           // Next order at same price level
    Order* prev_at_price;           // Previous order at same price level
    
    Order() 
        : id(INVALID_ORDER_ID)
        , side(Side::Buy)
        , type(OrderType::Limit)
        , tif(TimeInForce::Day)
        , status(OrderStatus::New)
        , submit_time(0)
        , last_update_time(0)
        , expiry_time(0)
        , avg_fill_price(0.0)
        , next_at_price(nullptr)
        , prev_at_price(nullptr) {}
    
    // Create a new order
    static Order create(const Symbol& sym, Side s, OrderType t,
                        Quantity qty, Price limit_price = Price::zero());
    
    // Check if order is active (can be filled or cancelled)
    bool is_active() const {
        return status == OrderStatus::New || 
               status == OrderStatus::PartiallyFilled;
    }
    
    // Check if order is complete
    bool is_complete() const {
        return status == OrderStatus::Filled ||
               status == OrderStatus::Cancelled ||
               status == OrderStatus::Rejected ||
               status == OrderStatus::Expired;
    }
    
    // Calculate notional value
    double notional() const {
        return price.to_double() * quantity.to_double();
    }
    
    // Calculate filled notional
    double filled_notional() const {
        return avg_fill_price * filled_quantity.to_double();
    }
    
    // Process a fill
    void apply_fill(Quantity fill_qty, Price fill_price);

    // Cancel the order
    void cancel();

    // Reject the order
    void reject(const std::string& reason = "");
};

// ============================================================================
// Order Request - Used for submitting new orders
// ============================================================================
struct OrderRequest {
    Symbol symbol;
    Side side;
    OrderType type;
    TimeInForce tif;
    Quantity quantity;
    Price price;                    // For limit orders
    Price stop_price;               // For stop orders
    std::string client_order_id;
    std::string strategy_id;
    
    OrderRequest() 
        : side(Side::Buy)
        , type(OrderType::Market)
        , tif(TimeInForce::Day) {}
    
    // Convenience constructors
    static OrderRequest market(const Symbol& sym, Side s, Quantity qty,
                               const std::string& strategy = "");

    static OrderRequest limit(const Symbol& sym, Side s, Quantity qty, Price price,
                              const std::string& strategy = "");

    static OrderRequest ioc(const Symbol& sym, Side s, Quantity qty, Price price,
                            const std::string& strategy = "");
};

// ============================================================================
// Cancel Request
// ============================================================================
struct CancelRequest {
    OrderId order_id;
    std::string client_order_id;
    std::string strategy_id;
    
    CancelRequest() : order_id(INVALID_ORDER_ID) {}
    
    explicit CancelRequest(OrderId id) : order_id(id) {}
};

}  // namespace backtest
