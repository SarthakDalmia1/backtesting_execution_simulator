#include "test_framework.hpp"
#include "core/types.hpp"
#include "orderbook/order.hpp"
#include "orderbook/matching_engine.hpp"

using namespace backtest;

// ============================================================================
// Matching Engine Tests
// ============================================================================

TEST_CASE(matching_limit_order_no_match) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    OrderRequest req;
    req.symbol = symbol;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.quantity = Quantity::from_int(100);
    req.price = Price::from_double(100.0);
    
    Order* order = engine.submit_order(req);
    
    ASSERT_NE(order, nullptr);
    ASSERT_EQ(order->status, OrderStatus::New);
    ASSERT_TRUE(order->remaining_quantity == req.quantity);
}

TEST_CASE(matching_market_order_no_liquidity) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    OrderRequest req;
    req.symbol = symbol;
    req.side = Side::Buy;
    req.type = OrderType::Market;
    req.quantity = Quantity::from_int(100);
    
    Order* order = engine.submit_order(req);
    
    ASSERT_NE(order, nullptr);
    // Market order with no liquidity should be cancelled
    ASSERT_EQ(order->status, OrderStatus::Cancelled);
}

TEST_CASE(matching_limit_order_full_match) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    int fill_count = 0;
    engine.set_fill_callback([&fill_count](const Fill&) {
        ++fill_count;
    });
    
    // Add passive sell order
    OrderRequest sell_req;
    sell_req.symbol = symbol;
    sell_req.side = Side::Sell;
    sell_req.type = OrderType::Limit;
    sell_req.quantity = Quantity::from_int(100);
    sell_req.price = Price::from_double(100.0);
    
    Order* sell_order = engine.submit_order(sell_req);
    ASSERT_EQ(sell_order->status, OrderStatus::New);
    
    // Add aggressive buy order at same price
    OrderRequest buy_req;
    buy_req.symbol = symbol;
    buy_req.side = Side::Buy;
    buy_req.type = OrderType::Limit;
    buy_req.quantity = Quantity::from_int(100);
    buy_req.price = Price::from_double(100.0);
    
    Order* buy_order = engine.submit_order(buy_req);
    
    // Both orders should be filled
    ASSERT_EQ(sell_order->status, OrderStatus::Filled);
    ASSERT_EQ(buy_order->status, OrderStatus::Filled);
    ASSERT_EQ(fill_count, 2);  // One fill for each side
}

TEST_CASE(matching_limit_order_partial_fill) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    // Add passive sell order for 50 shares
    OrderRequest sell_req;
    sell_req.symbol = symbol;
    sell_req.side = Side::Sell;
    sell_req.type = OrderType::Limit;
    sell_req.quantity = Quantity::from_int(50);
    sell_req.price = Price::from_double(100.0);
    
    engine.submit_order(sell_req);
    
    // Add aggressive buy order for 100 shares
    OrderRequest buy_req;
    buy_req.symbol = symbol;
    buy_req.side = Side::Buy;
    buy_req.type = OrderType::Limit;
    buy_req.quantity = Quantity::from_int(100);
    buy_req.price = Price::from_double(100.0);
    
    Order* buy_order = engine.submit_order(buy_req);
    
    // Buy order should be partially filled
    ASSERT_EQ(buy_order->status, OrderStatus::PartiallyFilled);
    ASSERT_EQ(buy_order->filled_quantity.to_int(), 50);
    ASSERT_EQ(buy_order->remaining_quantity.to_int(), 50);
}

TEST_CASE(matching_price_improvement) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    Price fill_price;
    engine.set_fill_callback([&fill_price](const Fill& fill) {
        fill_price = fill.fill_price;
    });
    
    // Add passive sell order at 100
    OrderRequest sell_req;
    sell_req.symbol = symbol;
    sell_req.side = Side::Sell;
    sell_req.type = OrderType::Limit;
    sell_req.quantity = Quantity::from_int(100);
    sell_req.price = Price::from_double(100.0);
    
    engine.submit_order(sell_req);
    
    // Add aggressive buy order at 101 (willing to pay more)
    OrderRequest buy_req;
    buy_req.symbol = symbol;
    buy_req.side = Side::Buy;
    buy_req.type = OrderType::Limit;
    buy_req.quantity = Quantity::from_int(100);
    buy_req.price = Price::from_double(101.0);
    
    engine.submit_order(buy_req);
    
    // Fill should be at passive order price (price-time priority)
    ASSERT_NEAR(fill_price.to_double(), 100.0, 0.000001);
}

TEST_CASE(matching_cancel_order) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    OrderRequest req;
    req.symbol = symbol;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.quantity = Quantity::from_int(100);
    req.price = Price::from_double(100.0);
    
    Order* order = engine.submit_order(req);
    OrderId order_id = order->id;
    
    ASSERT_TRUE(engine.cancel_order(order_id));
    ASSERT_EQ(order->status, OrderStatus::Cancelled);
}

TEST_CASE(matching_ioc_full_fill) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    // Add passive liquidity
    OrderRequest sell_req;
    sell_req.symbol = symbol;
    sell_req.side = Side::Sell;
    sell_req.type = OrderType::Limit;
    sell_req.quantity = Quantity::from_int(100);
    sell_req.price = Price::from_double(100.0);
    
    engine.submit_order(sell_req);
    
    // Submit IOC order
    OrderRequest ioc_req;
    ioc_req.symbol = symbol;
    ioc_req.side = Side::Buy;
    ioc_req.type = OrderType::IOC;
    ioc_req.quantity = Quantity::from_int(100);
    ioc_req.price = Price::from_double(100.0);
    
    Order* ioc_order = engine.submit_order(ioc_req);
    
    ASSERT_EQ(ioc_order->status, OrderStatus::Filled);
}

TEST_CASE(matching_ioc_partial_cancel) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    // Add partial liquidity
    OrderRequest sell_req;
    sell_req.symbol = symbol;
    sell_req.side = Side::Sell;
    sell_req.type = OrderType::Limit;
    sell_req.quantity = Quantity::from_int(50);
    sell_req.price = Price::from_double(100.0);
    
    engine.submit_order(sell_req);
    
    // Submit IOC order for more than available
    OrderRequest ioc_req;
    ioc_req.symbol = symbol;
    ioc_req.side = Side::Buy;
    ioc_req.type = OrderType::IOC;
    ioc_req.quantity = Quantity::from_int(100);
    ioc_req.price = Price::from_double(100.0);
    
    Order* ioc_order = engine.submit_order(ioc_req);
    
    // IOC should be cancelled after partial fill
    ASSERT_EQ(ioc_order->status, OrderStatus::Cancelled);
    ASSERT_EQ(ioc_order->filled_quantity.to_int(), 50);
}

TEST_CASE(matching_fok_success) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    // Add enough liquidity
    OrderRequest sell_req;
    sell_req.symbol = symbol;
    sell_req.side = Side::Sell;
    sell_req.type = OrderType::Limit;
    sell_req.quantity = Quantity::from_int(100);
    sell_req.price = Price::from_double(100.0);
    
    engine.submit_order(sell_req);
    
    // Submit FOK order
    OrderRequest fok_req;
    fok_req.symbol = symbol;
    fok_req.side = Side::Buy;
    fok_req.type = OrderType::FOK;
    fok_req.quantity = Quantity::from_int(100);
    fok_req.price = Price::from_double(100.0);
    
    Order* fok_order = engine.submit_order(fok_req);
    
    ASSERT_EQ(fok_order->status, OrderStatus::Filled);
}

TEST_CASE(matching_fok_reject) {
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    // Add insufficient liquidity
    OrderRequest sell_req;
    sell_req.symbol = symbol;
    sell_req.side = Side::Sell;
    sell_req.type = OrderType::Limit;
    sell_req.quantity = Quantity::from_int(50);
    sell_req.price = Price::from_double(100.0);
    
    engine.submit_order(sell_req);
    
    // Submit FOK order for more than available
    OrderRequest fok_req;
    fok_req.symbol = symbol;
    fok_req.side = Side::Buy;
    fok_req.type = OrderType::FOK;
    fok_req.quantity = Quantity::from_int(100);
    fok_req.price = Price::from_double(100.0);
    
    Order* fok_order = engine.submit_order(fok_req);
    
    // FOK should be rejected
    ASSERT_EQ(fok_order->status, OrderStatus::Rejected);
    ASSERT_TRUE(fok_order->filled_quantity.is_zero());
}
