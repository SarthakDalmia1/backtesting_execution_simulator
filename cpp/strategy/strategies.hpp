#pragma once

#include "strategy/strategy_base.hpp"
#include "core/utils.hpp"
#include <deque>
#include <cmath>

namespace backtest {

// ============================================================================
// VWAP Execution Strategy
// ============================================================================
// Executes a large order by targeting the Volume Weighted Average Price
class VWAPStrategy : public StrategyBase {
public:
    VWAPStrategy() : StrategyBase("VWAP") {}
    
    // Configuration
    void configure(const Symbol& symbol, Side side, Quantity total_qty,
                   Timestamp start_time, Timestamp end_time, int num_slices = 10);

    void on_tick(const Tick& tick) override;

    void on_fill(const Fill& fill) override;

    // Get execution statistics
    double execution_vwap() const {
        double filled_qty = (total_quantity_ - remaining_quantity_).to_double();
        return filled_qty > 0 ? filled_notional_ / filled_qty : 0.0;
    }

    double market_vwap() const {
        return total_volume_ > 0 ? total_pv_ / total_volume_ : 0.0;
    }

    double slippage_vs_vwap() const;

private:
    void execute_slice(const Tick& tick);

    void update_vwap(const Tick& tick);

    
    Symbol symbol_;
    Side side_;
    Quantity total_quantity_;
    Quantity remaining_quantity_;
    Quantity slice_quantity_;
    Timestamp start_time_;
    Timestamp end_time_;
    int64_t time_interval_;
    Timestamp next_slice_time_;
    int num_slices_;
    int slices_sent_ = 0;
    
    double filled_notional_ = 0.0;
    double total_pv_ = 0.0;
    double total_volume_ = 0.0;
};

// ============================================================================
// Mean Reversion Strategy
// ============================================================================
// Trades when price deviates from moving average
class MeanReversionStrategy : public StrategyBase {
public:
    MeanReversionStrategy() : StrategyBase("MeanReversion") {}
    
    // Configuration
    void configure(const Symbol& symbol, int lookback_period = 20,
                   double entry_z_score = 2.0, double exit_z_score = 0.5,
                   Quantity position_size = Quantity::from_int(100));

    void on_tick(const Tick& tick) override;

    // Get current statistics
    double current_mean() const;

    double current_z_score() const;

private:
    Symbol symbol_;
    int lookback_period_ = 20;
    double entry_z_score_ = 2.0;
    double exit_z_score_ = 0.5;
    Quantity position_size_;
    
    std::deque<double> prices_;
};

// ============================================================================
// Simple Market Making Strategy
// ============================================================================
// Posts bids and offers around the mid-price
class MarketMakingStrategy : public StrategyBase {
public:
    MarketMakingStrategy() : StrategyBase("MarketMaking") {}
    
    // Configuration
    void configure(const Symbol& symbol, double spread_bps = 10.0,
                   Quantity order_size = Quantity::from_int(100),
                   int max_position = 1000, double skew_factor = 0.1);

    void on_tick(const Tick& tick) override;

    void on_fill(const Fill& fill) override;
    
    // Get statistics
    int buy_fills() const { return buy_fills_; }
    int sell_fills() const { return sell_fills_; }
    int total_fills() const { return buy_fills_ + sell_fills_; }

private:
    Symbol symbol_;
    double half_spread_bps_ = 5.0;
    Quantity order_size_;
    int max_position_ = 1000;
    double skew_factor_ = 0.1;
    
    OrderId bid_order_id_ = INVALID_ORDER_ID;
    OrderId ask_order_id_ = INVALID_ORDER_ID;
    
    int buy_fills_ = 0;
    int sell_fills_ = 0;
};

// ============================================================================
// Momentum Strategy
// ============================================================================
// Follows price momentum over a lookback period
class MomentumStrategy : public StrategyBase {
public:
    MomentumStrategy() : StrategyBase("Momentum") {}
    
    void configure(const Symbol& symbol, int lookback_period = 10,
                   double threshold_bps = 50.0,
                   Quantity position_size = Quantity::from_int(100));

    void on_tick(const Tick& tick) override;

private:
    Symbol symbol_;
    int lookback_period_ = 10;
    double threshold_bps_ = 50.0;
    Quantity position_size_;
    std::deque<double> prices_;
};

}  // namespace backtest
