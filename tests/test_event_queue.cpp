#include "test_framework.hpp"
#include "core/types.hpp"
#include "events/event.hpp"
#include "events/event_queue.hpp"

using namespace backtest;

// ============================================================================
// Event Queue Tests
// ============================================================================

TEST_CASE(event_queue_empty) {
    EventQueue queue;
    
    ASSERT_TRUE(queue.empty());
    ASSERT_EQ(queue.size(), 0);
}

TEST_CASE(event_queue_push_pop) {
    EventQueue queue;
    
    Tick tick;
    tick.timestamp = 1000;
    tick.symbol = Symbol("AAPL");
    
    MarketDataEvent event(tick);
    queue.push(event);
    
    ASSERT_FALSE(queue.empty());
    ASSERT_EQ(queue.size(), 1);
    
    auto popped = queue.pop();
    ASSERT_TRUE(popped.has_value());
    ASSERT_TRUE(queue.empty());
}

TEST_CASE(event_queue_ordering) {
    EventQueue queue;
    
    // Add events out of order
    Tick tick1, tick2, tick3;
    tick1.timestamp = 3000;
    tick2.timestamp = 1000;
    tick3.timestamp = 2000;
    
    queue.push(MarketDataEvent(tick1));
    queue.push(MarketDataEvent(tick2));
    queue.push(MarketDataEvent(tick3));
    
    // Should pop in timestamp order
    auto e1 = queue.pop();
    auto e2 = queue.pop();
    auto e3 = queue.pop();
    
    ASSERT_TRUE(e1.has_value());
    ASSERT_TRUE(e2.has_value());
    ASSERT_TRUE(e3.has_value());
    
    ASSERT_EQ(get_event_timestamp(*e1), 1000);
    ASSERT_EQ(get_event_timestamp(*e2), 2000);
    ASSERT_EQ(get_event_timestamp(*e3), 3000);
}

TEST_CASE(event_queue_next_timestamp) {
    EventQueue queue;
    
    Tick tick;
    tick.timestamp = 5000;
    queue.push(MarketDataEvent(tick));
    
    ASSERT_EQ(queue.next_timestamp(), 5000);
}

TEST_CASE(event_queue_peek) {
    EventQueue queue;
    
    Tick tick;
    tick.timestamp = 1000;
    tick.symbol = Symbol("AAPL");
    queue.push(MarketDataEvent(tick));
    
    const Event* peeked = queue.peek();
    ASSERT_NE(peeked, nullptr);
    
    // Queue should still have the event
    ASSERT_FALSE(queue.empty());
    ASSERT_EQ(queue.size(), 1);
}

TEST_CASE(event_queue_clear) {
    EventQueue queue;
    
    Tick tick;
    tick.timestamp = 1000;
    
    queue.push(MarketDataEvent(tick));
    queue.push(MarketDataEvent(tick));
    queue.push(MarketDataEvent(tick));
    
    ASSERT_EQ(queue.size(), 3);
    
    queue.clear();
    
    ASSERT_TRUE(queue.empty());
    ASSERT_EQ(queue.size(), 0);
}

// ============================================================================
// Event Type Tests
// ============================================================================

TEST_CASE(event_type_market_data) {
    Tick tick;
    tick.timestamp = 1000;
    tick.symbol = Symbol("AAPL");
    tick.bid_price = Price::from_double(100.0);
    tick.ask_price = Price::from_double(100.01);
    
    MarketDataEvent event(tick);
    
    ASSERT_EQ(event.type, EventType::MarketData);
    ASSERT_EQ(event.timestamp, 1000);
    ASSERT_TRUE(event.tick.symbol == Symbol("AAPL"));
}

TEST_CASE(event_type_order_fill) {
    Fill fill;
    fill.order_id = 123;
    fill.trade_id = 456;
    fill.timestamp = 2000;
    fill.fill_price = Price::from_double(100.0);
    fill.fill_quantity = Quantity::from_int(100);
    fill.side = Side::Buy;
    fill.is_maker = false;
    
    OrderFillEvent event(2000, fill, 0.5);
    
    ASSERT_EQ(event.type, EventType::OrderFill);
    ASSERT_EQ(event.fill.order_id, 123);
    ASSERT_NEAR(event.transaction_cost, 0.5, 0.001);
}

TEST_CASE(event_variant_visitor) {
    Tick tick;
    tick.timestamp = 1000;
    tick.symbol = Symbol("AAPL");
    
    Event event = MarketDataEvent(tick);
    
    bool visited = false;
    std::visit([&visited](auto&& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, MarketDataEvent>) {
            visited = true;
        }
    }, event);
    
    ASSERT_TRUE(visited);
}
