#include "test_framework.hpp"
#include "core/types.hpp"
#include "strategy/strategy_base.hpp"
#include "strategy/strategies.hpp"
#include "execution/execution_simulator.hpp"
#include "data/market_data_feed.hpp"

using namespace backtest;

// ============================================================================
// Strategy Tests
// ============================================================================

TEST_CASE(vwap_strategy_basic) {
    VWAPStrategy strategy;
    
    Symbol symbol("AAPL");
    Timestamp start = to_timestamp(2024, 1, 1, 9, 30);
    Timestamp end = to_timestamp(2024, 1, 1, 10, 0);
    
    strategy.configure(symbol, Side::Buy, Quantity::from_int(1000),
                       start, end, 10);
    
    ASSERT_TRUE(strategy.is_active());
    ASSERT_EQ(strategy.name(), "VWAP");
}

TEST_CASE(mean_reversion_strategy_basic) {
    MeanReversionStrategy strategy;
    
    Symbol symbol("AAPL");
    strategy.configure(symbol, 20, 2.0, 0.5, Quantity::from_int(100));
    
    ASSERT_TRUE(strategy.is_active());
    ASSERT_EQ(strategy.name(), "MeanReversion");
}

TEST_CASE(market_making_strategy_basic) {
    MarketMakingStrategy strategy;
    
    Symbol symbol("AAPL");
    strategy.configure(symbol, 10.0, Quantity::from_int(100), 1000, 0.1);
    
    ASSERT_TRUE(strategy.is_active());
    ASSERT_EQ(strategy.name(), "MarketMaking");
}

TEST_CASE(momentum_strategy_basic) {
    MomentumStrategy strategy;
    
    Symbol symbol("AAPL");
    strategy.configure(symbol, 10, 50.0, Quantity::from_int(100));
    
    ASSERT_TRUE(strategy.is_active());
    ASSERT_EQ(strategy.name(), "Momentum");
}

// ============================================================================
// Integration Test: Strategy with Simulator
// ============================================================================

TEST_CASE(integration_simple_backtest) {
    // Setup
    BacktestConfig config;
    config.initial_capital = 100000.0;
    config.start_time = to_timestamp(2024, 1, 1, 9, 30);
    config.end_time = to_timestamp(2024, 1, 1, 10, 0);
    config.latency.market_data_latency_ns = 0;
    config.latency.order_submit_latency_ns = 0;
    
    ExecutionSimulator simulator(config);
    
    // Generate test data
    Symbol symbol("AAPL");
    int64_t interval = NANOSECONDS_PER_SECOND;
    SyntheticTickGenerator generator(symbol, 100.0, 0.01, 
                                     config.start_time, config.end_time, interval);
    
    // Load ticks
    Tick tick;
    while (generator.next_tick(tick)) {
        simulator.add_tick(tick);
    }
    
    // Run simulation
    simulator.run();
    
    ASSERT_GT(simulator.total_events_processed(), 0);
}
