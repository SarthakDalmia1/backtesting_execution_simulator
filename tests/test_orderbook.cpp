#include "test_framework.hpp"
#include "core/types.hpp"
#include "core/memory_pool.hpp"
#include "orderbook/order.hpp"
#include "orderbook/orderbook.hpp"

using namespace backtest;

// ============================================================================
// Price Tests
// ============================================================================

TEST_CASE(price_from_double) {
    Price p = Price::from_double(100.50);
    ASSERT_NEAR(p.to_double(), 100.50, 0.000001);
}

TEST_CASE(price_arithmetic) {
    Price p1 = Price::from_double(100.0);
    Price p2 = Price::from_double(50.0);
    
    Price sum = p1 + p2;
    ASSERT_NEAR(sum.to_double(), 150.0, 0.000001);
    
    Price diff = p1 - p2;
    ASSERT_NEAR(diff.to_double(), 50.0, 0.000001);
}

TEST_CASE(price_comparison) {
    Price p1 = Price::from_double(100.0);
    Price p2 = Price::from_double(101.0);
    
    ASSERT_TRUE(p1 < p2);
    ASSERT_TRUE(p2 > p1);
    ASSERT_TRUE(p1 <= p1);
    ASSERT_TRUE(p1 >= p1);
    ASSERT_FALSE(p1 == p2);
}

// ============================================================================
// Quantity Tests
// ============================================================================

TEST_CASE(quantity_from_int) {
    Quantity q = Quantity::from_int(100);
    ASSERT_EQ(q.to_int(), 100);
}

TEST_CASE(quantity_from_double) {
    Quantity q = Quantity::from_double(100.5);
    ASSERT_NEAR(q.to_double(), 100.5, 0.0001);
}

TEST_CASE(quantity_arithmetic) {
    Quantity q1 = Quantity::from_int(100);
    Quantity q2 = Quantity::from_int(50);
    
    Quantity sum = q1 + q2;
    ASSERT_EQ(sum.to_int(), 150);
    
    Quantity diff = q1 - q2;
    ASSERT_EQ(diff.to_int(), 50);
}

TEST_CASE(quantity_is_zero) {
    Quantity q1 = Quantity::from_int(0);
    Quantity q2 = Quantity::from_int(100);
    
    ASSERT_TRUE(q1.is_zero());
    ASSERT_FALSE(q2.is_zero());
}

// ============================================================================
// Symbol Tests
// ============================================================================

TEST_CASE(symbol_construction) {
    Symbol s1("AAPL");
    ASSERT_EQ(s1.to_string(), "AAPL");
    
    Symbol s2(std::string("MSFT"));
    ASSERT_EQ(s2.to_string(), "MSFT");
}

TEST_CASE(symbol_comparison) {
    Symbol s1("AAPL");
    Symbol s2("AAPL");
    Symbol s3("MSFT");
    
    ASSERT_TRUE(s1 == s2);
    ASSERT_FALSE(s1 == s3);
    ASSERT_TRUE(s1 != s3);
}

// ============================================================================
// Order Book Tests
// ============================================================================

TEST_CASE(orderbook_empty) {
    OrderBook book(Symbol("AAPL"));
    
    ASSERT_TRUE(book.bids().empty());
    ASSERT_TRUE(book.asks().empty());
    ASSERT_EQ(book.order_count(), 0);
}

TEST_CASE(orderbook_add_bid) {
    OrderBook book(Symbol("AAPL"));
    
    Order order;
    order.id = 1;
    order.symbol = Symbol("AAPL");
    order.side = Side::Buy;
    order.price = Price::from_double(100.0);
    order.quantity = Quantity::from_int(100);
    order.remaining_quantity = order.quantity;
    
    book.add_order(&order);
    
    ASSERT_FALSE(book.bids().empty());
    ASSERT_TRUE(book.asks().empty());
    ASSERT_EQ(book.order_count(), 1);
    ASSERT_NEAR(book.best_bid().to_double(), 100.0, 0.000001);
}

TEST_CASE(orderbook_add_ask) {
    OrderBook book(Symbol("AAPL"));
    
    Order order;
    order.id = 1;
    order.symbol = Symbol("AAPL");
    order.side = Side::Sell;
    order.price = Price::from_double(101.0);
    order.quantity = Quantity::from_int(100);
    order.remaining_quantity = order.quantity;
    
    book.add_order(&order);
    
    ASSERT_TRUE(book.bids().empty());
    ASSERT_FALSE(book.asks().empty());
    ASSERT_NEAR(book.best_ask().to_double(), 101.0, 0.000001);
}

TEST_CASE(orderbook_bid_priority) {
    OrderBook book(Symbol("AAPL"));
    
    Order order1, order2, order3;
    
    order1.id = 1;
    order1.side = Side::Buy;
    order1.price = Price::from_double(100.0);
    order1.remaining_quantity = Quantity::from_int(100);
    
    order2.id = 2;
    order2.side = Side::Buy;
    order2.price = Price::from_double(101.0);  // Higher price
    order2.remaining_quantity = Quantity::from_int(100);
    
    order3.id = 3;
    order3.side = Side::Buy;
    order3.price = Price::from_double(99.0);
    order3.remaining_quantity = Quantity::from_int(100);
    
    book.add_order(&order1);
    book.add_order(&order2);
    book.add_order(&order3);
    
    // Best bid should be highest price
    ASSERT_NEAR(book.best_bid().to_double(), 101.0, 0.000001);
}

TEST_CASE(orderbook_ask_priority) {
    OrderBook book(Symbol("AAPL"));
    
    Order order1, order2, order3;
    
    order1.id = 1;
    order1.side = Side::Sell;
    order1.price = Price::from_double(101.0);
    order1.remaining_quantity = Quantity::from_int(100);
    
    order2.id = 2;
    order2.side = Side::Sell;
    order2.price = Price::from_double(100.0);  // Lower price
    order2.remaining_quantity = Quantity::from_int(100);
    
    order3.id = 3;
    order3.side = Side::Sell;
    order3.price = Price::from_double(102.0);
    order3.remaining_quantity = Quantity::from_int(100);
    
    book.add_order(&order1);
    book.add_order(&order2);
    book.add_order(&order3);
    
    // Best ask should be lowest price
    ASSERT_NEAR(book.best_ask().to_double(), 100.0, 0.000001);
}

TEST_CASE(orderbook_spread) {
    OrderBook book(Symbol("AAPL"));
    
    Order bid, ask;
    
    bid.id = 1;
    bid.side = Side::Buy;
    bid.price = Price::from_double(100.0);
    bid.remaining_quantity = Quantity::from_int(100);
    
    ask.id = 2;
    ask.side = Side::Sell;
    ask.price = Price::from_double(101.0);
    ask.remaining_quantity = Quantity::from_int(100);
    
    book.add_order(&bid);
    book.add_order(&ask);
    
    ASSERT_NEAR(book.spread().to_double(), 1.0, 0.000001);
    ASSERT_NEAR(book.mid_price().to_double(), 100.5, 0.000001);
}

TEST_CASE(orderbook_remove_order) {
    OrderBook book(Symbol("AAPL"));
    
    Order order;
    order.id = 1;
    order.side = Side::Buy;
    order.price = Price::from_double(100.0);
    order.remaining_quantity = Quantity::from_int(100);
    
    book.add_order(&order);
    ASSERT_EQ(book.order_count(), 1);
    
    book.remove_order(&order);
    ASSERT_EQ(book.order_count(), 0);
    ASSERT_TRUE(book.bids().empty());
}
