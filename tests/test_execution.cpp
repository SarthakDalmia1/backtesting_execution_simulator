#include "test_framework.hpp"
#include "core/types.hpp"
#include "execution/slippage_model.hpp"
#include "execution/transaction_costs.hpp"
#include "execution/execution_simulator.hpp"
#include "data/market_data_feed.hpp"

using namespace backtest;

// ============================================================================
// Slippage Model Tests
// ============================================================================

TEST_CASE(zero_slippage_model) {
    ZeroSlippageModel model;
    
    Price slip = model.calculate_slippage(
        Price::from_double(100.0),
        Quantity::from_int(100),
        Quantity::from_int(1000),
        Side::Buy,
        0.02,
        1000000.0
    );
    
    ASSERT_NEAR(slip.to_double(), 0.0, 0.000001);
}

TEST_CASE(fixed_slippage_model) {
    FixedSlippageModel model(5.0);  // 5 bps
    
    Price base = Price::from_double(100.0);
    
    Price buy_slip = model.calculate_slippage(
        base, Quantity::from_int(100), Quantity::from_int(1000), Side::Buy, 0, 0);
    
    Price sell_slip = model.calculate_slippage(
        base, Quantity::from_int(100), Quantity::from_int(1000), Side::Sell, 0, 0);
    
    // Buy should slip up, sell should slip down
    ASSERT_GT(buy_slip.to_double(), 0.0);
    ASSERT_LT(sell_slip.to_double(), 0.0);
    
    // Should be approximately 5 bps = 0.05%
    ASSERT_NEAR(std::abs(buy_slip.to_double()), 0.05, 0.001);
}

TEST_CASE(volume_slippage_model) {
    VolumeSlippageModel model(0.5, 0.1, 0.5);
    
    Price base = Price::from_double(100.0);
    
    // Small order should have small slippage
    Price small_slip = model.calculate_slippage(
        base, Quantity::from_int(100), Quantity::from_int(10000),
        Side::Buy, 0.02, 1000000.0);
    
    // Large order should have larger slippage
    Price large_slip = model.calculate_slippage(
        base, Quantity::from_int(10000), Quantity::from_int(10000),
        Side::Buy, 0.02, 1000000.0);
    
    ASSERT_GT(large_slip.to_double(), small_slip.to_double());
}

// ============================================================================
// Transaction Cost Tests
// ============================================================================

TEST_CASE(zero_cost_model) {
    ZeroCostModel model;
    
    double cost = model.calculate_cost(10000.0, Quantity::from_int(100), false, Symbol("AAPL"));
    
    ASSERT_NEAR(cost, 0.0, 0.001);
}

TEST_CASE(percentage_cost_model_taker) {
    PercentageCostModel model(0.0, 1.0, 0.0);  // 0 bps maker, 1 bps taker
    
    double cost = model.calculate_cost(10000.0, Quantity::from_int(100), false, Symbol("AAPL"));
    
    // 1 bps of 10000 = 1.0
    ASSERT_NEAR(cost, 1.0, 0.001);
}

TEST_CASE(percentage_cost_model_maker) {
    PercentageCostModel model(0.5, 1.0, 0.0);  // 0.5 bps maker
    
    double cost = model.calculate_cost(10000.0, Quantity::from_int(100), true, Symbol("AAPL"));
    
    // 0.5 bps of 10000 = 0.5
    ASSERT_NEAR(cost, 0.5, 0.001);
}

TEST_CASE(percentage_cost_model_min_cost) {
    PercentageCostModel model(0.0, 1.0, 5.0);  // min cost 5.0
    
    // Small trade
    double cost = model.calculate_cost(100.0, Quantity::from_int(1), false, Symbol("AAPL"));
    
    // Should be min cost
    ASSERT_NEAR(cost, 5.0, 0.001);
}

// ============================================================================
// Execution Simulator Tests
// ============================================================================

TEST_CASE(simulator_basic_setup) {
    BacktestConfig config;
    config.initial_capital = 1000000.0;
    config.start_time = to_timestamp(2024, 1, 1, 9, 30);
    config.end_time = to_timestamp(2024, 1, 1, 16, 0);
    
    ExecutionSimulator simulator(config);
    
    ASSERT_NEAR(simulator.position_manager().cash(), 1000000.0, 0.01);
}

TEST_CASE(simulator_add_tick) {
    BacktestConfig config;
    config.initial_capital = 1000000.0;
    config.start_time = to_timestamp(2024, 1, 1, 9, 30);
    
    ExecutionSimulator simulator(config);
    
    Tick tick;
    tick.timestamp = config.start_time + NANOSECONDS_PER_SECOND;
    tick.symbol = Symbol("AAPL");
    tick.bid_price = Price::from_double(100.0);
    tick.ask_price = Price::from_double(100.01);
    
    simulator.add_tick(tick);
    
    ASSERT_FALSE(simulator.event_queue().empty());
}

TEST_CASE(simulator_process_tick) {
    BacktestConfig config;
    config.initial_capital = 1000000.0;
    config.start_time = to_timestamp(2024, 1, 1, 9, 30);
    config.latency.market_data_latency_ns = 0;  // No latency for testing
    
    ExecutionSimulator simulator(config);
    
    int tick_count = 0;
    simulator.set_event_callback([&tick_count](const Event& event) {
        if (get_event_type(event) == EventType::MarketData) {
            ++tick_count;
        }
    });
    
    Tick tick;
    tick.timestamp = config.start_time + NANOSECONDS_PER_SECOND;
    tick.symbol = Symbol("AAPL");
    tick.bid_price = Price::from_double(100.0);
    tick.ask_price = Price::from_double(100.01);
    
    simulator.add_tick(tick);
    simulator.process_next_event();
    
    ASSERT_EQ(tick_count, 1);
}

// ============================================================================
// Synthetic Data Feed Tests
// ============================================================================

TEST_CASE(synthetic_generator_basic) {
    Symbol symbol("AAPL");
    Timestamp start = to_timestamp(2024, 1, 1, 9, 30);
    Timestamp end = to_timestamp(2024, 1, 1, 9, 31);
    int64_t interval = NANOSECONDS_PER_SECOND;  // 1 tick per second
    
    SyntheticTickGenerator generator(symbol, 100.0, 0.02, start, end, interval);
    
    ASSERT_TRUE(generator.has_data());
    ASSERT_EQ(generator.total_ticks(), 60);  // 60 seconds
    
    Tick tick;
    int count = 0;
    while (generator.next_tick(tick)) {
        ++count;
        ASSERT_TRUE(tick.symbol == symbol);
        ASSERT_GT(tick.bid_price.to_double(), 0.0);
        ASSERT_GT(tick.ask_price.to_double(), 0.0);
        ASSERT_GT(tick.ask_price.to_double(), tick.bid_price.to_double());
    }
    
    ASSERT_EQ(count, 60);
}

TEST_CASE(synthetic_generator_reset) {
    Symbol symbol("AAPL");
    Timestamp start = to_timestamp(2024, 1, 1, 9, 30);
    Timestamp end = to_timestamp(2024, 1, 1, 9, 31);
    
    SyntheticTickGenerator generator(symbol, 100.0, 0.02, start, end, NANOSECONDS_PER_SECOND);
    
    Tick tick;
    while (generator.next_tick(tick)) {}
    
    ASSERT_FALSE(generator.has_data());
    
    generator.reset();
    
    ASSERT_TRUE(generator.has_data());
}
