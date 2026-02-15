#include "test_framework.hpp"
#include "core/types.hpp"
#include "position/position_manager.hpp"
#include "position/pnl_tracker.hpp"

using namespace backtest;

// ============================================================================
// Position Tests
// ============================================================================

TEST_CASE(position_initial_state) {
    Position pos(Symbol("AAPL"));
    
    ASSERT_TRUE(pos.is_flat());
    ASSERT_FALSE(pos.is_long());
    ASSERT_FALSE(pos.is_short());
    ASSERT_NEAR(pos.avg_entry_price, 0.0, 0.001);
    ASSERT_NEAR(pos.realized_pnl, 0.0, 0.001);
}

TEST_CASE(position_manager_buy) {
    PositionManager pm(100000.0);
    Symbol symbol("AAPL");
    
    pm.apply_fill(symbol, Side::Buy, Quantity::from_int(100), Price::from_double(100.0));
    
    const Position& pos = pm.get_position(symbol);
    
    ASSERT_TRUE(pos.is_long());
    ASSERT_EQ(pos.quantity.to_int(), 100);
    ASSERT_NEAR(pos.avg_entry_price, 100.0, 0.01);
    ASSERT_NEAR(pm.cash(), 90000.0, 0.01);  // 100000 - 10000
}

TEST_CASE(position_manager_sell) {
    PositionManager pm(100000.0);
    Symbol symbol("AAPL");
    
    pm.apply_fill(symbol, Side::Sell, Quantity::from_int(100), Price::from_double(100.0));
    
    const Position& pos = pm.get_position(symbol);
    
    ASSERT_TRUE(pos.is_short());
    ASSERT_EQ(pos.quantity.to_int(), -100);
    ASSERT_NEAR(pm.cash(), 110000.0, 0.01);  // 100000 + 10000
}

TEST_CASE(position_manager_close_long) {
    PositionManager pm(100000.0);
    Symbol symbol("AAPL");
    
    // Buy 100 at 100
    pm.apply_fill(symbol, Side::Buy, Quantity::from_int(100), Price::from_double(100.0));
    
    // Sell 100 at 110 (profit)
    pm.apply_fill(symbol, Side::Sell, Quantity::from_int(100), Price::from_double(110.0));
    
    const Position& pos = pm.get_position(symbol);
    
    ASSERT_TRUE(pos.is_flat());
    ASSERT_NEAR(pos.realized_pnl, 1000.0, 0.01);  // 100 shares * $10 profit
    ASSERT_NEAR(pm.cash(), 101000.0, 0.01);  // 100000 - 10000 + 11000
}

TEST_CASE(position_manager_close_short) {
    PositionManager pm(100000.0);
    Symbol symbol("AAPL");
    
    // Short 100 at 100
    pm.apply_fill(symbol, Side::Sell, Quantity::from_int(100), Price::from_double(100.0));
    
    // Cover at 90 (profit)
    pm.apply_fill(symbol, Side::Buy, Quantity::from_int(100), Price::from_double(90.0));
    
    const Position& pos = pm.get_position(symbol);
    
    ASSERT_TRUE(pos.is_flat());
    ASSERT_NEAR(pos.realized_pnl, 1000.0, 0.01);  // 100 shares * $10 profit
}

TEST_CASE(position_manager_avg_price) {
    PositionManager pm(100000.0);
    Symbol symbol("AAPL");
    
    // Buy 100 at 100
    pm.apply_fill(symbol, Side::Buy, Quantity::from_int(100), Price::from_double(100.0));
    
    // Buy 100 more at 110
    pm.apply_fill(symbol, Side::Buy, Quantity::from_int(100), Price::from_double(110.0));
    
    const Position& pos = pm.get_position(symbol);
    
    ASSERT_EQ(pos.quantity.to_int(), 200);
    ASSERT_NEAR(pos.avg_entry_price, 105.0, 0.01);  // (10000 + 11000) / 200
}

TEST_CASE(position_manager_unrealized_pnl) {
    PositionManager pm(100000.0);
    Symbol symbol("AAPL");
    
    // Buy 100 at 100
    pm.apply_fill(symbol, Side::Buy, Quantity::from_int(100), Price::from_double(100.0));
    
    // Update price to 110
    pm.update_price(symbol, Price::from_double(110.0));
    
    const Position& pos = pm.get_position(symbol);
    
    ASSERT_NEAR(pos.unrealized_pnl, 1000.0, 0.01);  // 100 * (110 - 100)
}

TEST_CASE(position_manager_total_pnl) {
    PositionManager pm(100000.0);
    Symbol symbol("AAPL");
    
    // Buy 200 at 100
    pm.apply_fill(symbol, Side::Buy, Quantity::from_int(200), Price::from_double(100.0));
    
    // Sell 100 at 110 (realize profit)
    pm.apply_fill(symbol, Side::Sell, Quantity::from_int(100), Price::from_double(110.0));
    
    // Update remaining position price
    pm.update_price(symbol, Price::from_double(115.0));
    
    const Position& pos = pm.get_position(symbol);
    
    ASSERT_NEAR(pos.realized_pnl, 1000.0, 0.01);    // 100 * 10
    ASSERT_NEAR(pos.unrealized_pnl, 1500.0, 0.01);  // 100 * 15
    ASSERT_NEAR(pos.total_pnl(), 2500.0, 0.01);
}

// ============================================================================
// PnL Tracker Tests
// ============================================================================

TEST_CASE(pnl_tracker_initial_state) {
    PnLTracker tracker(100000.0);
    
    ASSERT_NEAR(tracker.initial_equity(), 100000.0, 0.01);
    ASSERT_NEAR(tracker.current_equity(), 100000.0, 0.01);
    ASSERT_NEAR(tracker.high_water_mark(), 100000.0, 0.01);
}

TEST_CASE(pnl_tracker_record_trade) {
    PnLTracker tracker(100000.0);
    
    tracker.record_trade(Symbol("AAPL"), Side::Buy, Quantity::from_int(100),
                        Price::from_double(100.0), 1.0);
    
    ASSERT_EQ(tracker.trades().size(), 1);
    ASSERT_NEAR(tracker.total_costs(), 1.0, 0.01);
}

TEST_CASE(pnl_tracker_calculate_stats) {
    PnLTracker tracker(100000.0);
    
    // Simulate some returns
    tracker.mark_to_market(101000.0);  // 1% gain
    tracker.mark_to_market(100500.0);  // -0.5% 
    tracker.mark_to_market(102000.0);  // +1.5%
    
    PerformanceStats stats = tracker.calculate_stats();
    
    ASSERT_GT(stats.total_return, 0.0);  // Should be positive
}

TEST_CASE(pnl_tracker_high_water_mark) {
    PnLTracker tracker(100000.0);
    
    tracker.mark_to_market(110000.0);  // New high
    ASSERT_NEAR(tracker.high_water_mark(), 110000.0, 0.01);
    
    tracker.mark_to_market(105000.0);  // Drawdown
    ASSERT_NEAR(tracker.high_water_mark(), 110000.0, 0.01);  // HWM unchanged
    
    tracker.mark_to_market(115000.0);  // New high
    ASSERT_NEAR(tracker.high_water_mark(), 115000.0, 0.01);
}
