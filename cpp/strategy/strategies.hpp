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
                   Timestamp start_time, Timestamp end_time, int num_slices = 10) {
        symbol_ = symbol;
        side_ = side;
        total_quantity_ = total_qty;
        remaining_quantity_ = total_qty;
        start_time_ = start_time;
        end_time_ = end_time;
        num_slices_ = num_slices;
        
        slice_quantity_ = Quantity(total_qty.raw / num_slices);
        time_interval_ = (end_time - start_time) / num_slices;
        next_slice_time_ = start_time;
        slices_sent_ = 0;
    }
    
    void on_tick(const Tick& tick) override {
        if (tick.symbol != symbol_ || !is_active_) return;
        
        Timestamp now = current_time();
        
        // Check if it's time to send next slice
        if (now >= next_slice_time_ && slices_sent_ < num_slices_) {
            execute_slice(tick);
        }
        
        // Update VWAP calculation
        update_vwap(tick);
    }
    
    void on_fill(const Fill& fill) override {
        remaining_quantity_ -= fill.fill_quantity;
        filled_notional_ += fill.fill_price.to_double() * fill.fill_quantity.to_double();
        
        // Track our execution VWAP
        if (remaining_quantity_.is_zero()) {
            is_active_ = false;
        }
    }
    
    // Get execution statistics
    double execution_vwap() const {
        double filled_qty = (total_quantity_ - remaining_quantity_).to_double();
        return filled_qty > 0 ? filled_notional_ / filled_qty : 0.0;
    }
    
    double market_vwap() const {
        return total_volume_ > 0 ? total_pv_ / total_volume_ : 0.0;
    }
    
    double slippage_vs_vwap() const {
        double exec_vwap = execution_vwap();
        double mkt_vwap = market_vwap();
        if (mkt_vwap <= 0) return 0.0;
        
        double slip = (exec_vwap - mkt_vwap) / mkt_vwap;
        return side_ == Side::Buy ? slip : -slip;  // Positive = bad
    }

private:
    void execute_slice(const Tick& tick) {
        // Determine slice size - proportional to expected volume
        Quantity qty = slice_quantity_;
        
        // Don't exceed remaining quantity
        if (qty > remaining_quantity_) {
            qty = remaining_quantity_;
        }
        
        if (qty.is_zero()) return;
        
        // Use IOC order to capture liquidity
        Price limit_price = side_ == Side::Buy ? tick.ask_price : tick.bid_price;
        submit_ioc_order(symbol_, side_, qty, limit_price);
        
        ++slices_sent_;
        next_slice_time_ += time_interval_;
    }
    
    void update_vwap(const Tick& tick) {
        // Track market VWAP (simplified using last price and implied volume)
        double price = tick.last_price.to_double();
        double volume = tick.last_size.to_double();
        
        total_pv_ += price * volume;
        total_volume_ += volume;
    }
    
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
                   Quantity position_size = Quantity::from_int(100)) {
        symbol_ = symbol;
        lookback_period_ = lookback_period;
        entry_z_score_ = entry_z_score;
        exit_z_score_ = exit_z_score;
        position_size_ = position_size;
    }
    
    void on_tick(const Tick& tick) override {
        if (tick.symbol != symbol_ || !is_active_) return;
        
        double mid = tick.mid_price().to_double();
        prices_.push_back(mid);
        
        // Keep only lookback_period prices
        while (prices_.size() > static_cast<size_t>(lookback_period_)) {
            prices_.pop_front();
        }
        
        // Need enough data
        if (prices_.size() < static_cast<size_t>(lookback_period_)) {
            return;
        }
        
        // Calculate mean and standard deviation
        double mean = math::sma(prices_, lookback_period_);
        double stddev = math::stddev(prices_);
        
        if (stddev < EPSILON) return;
        
        // Calculate z-score
        double z_score = (mid - mean) / stddev;
        
        const Position& pos = get_position(symbol_);
        
        // Trading logic
        if (pos.is_flat()) {
            // Look for entry
            if (z_score > entry_z_score_) {
                // Price is high - sell (expect reversion down)
                submit_market_order(symbol_, Side::Sell, position_size_);
            } else if (z_score < -entry_z_score_) {
                // Price is low - buy (expect reversion up)
                submit_market_order(symbol_, Side::Buy, position_size_);
            }
        } else if (pos.is_long()) {
            // Exit long when z-score returns to normal or reverses
            if (z_score > -exit_z_score_) {
                submit_market_order(symbol_, Side::Sell, pos.abs_quantity());
            }
        } else {  // Short
            // Exit short when z-score returns to normal or reverses
            if (z_score < exit_z_score_) {
                submit_market_order(symbol_, Side::Buy, pos.abs_quantity());
            }
        }
    }
    
    // Get current statistics
    double current_mean() const {
        if (prices_.size() < static_cast<size_t>(lookback_period_)) return 0.0;
        return math::sma(prices_, lookback_period_);
    }
    
    double current_z_score() const {
        if (prices_.size() < static_cast<size_t>(lookback_period_)) return 0.0;
        double mean = math::sma(prices_, lookback_period_);
        double std = math::stddev(prices_);
        if (std < EPSILON) return 0.0;
        return (prices_.back() - mean) / std;
    }

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
                   int max_position = 1000, double skew_factor = 0.1) {
        symbol_ = symbol;
        half_spread_bps_ = spread_bps / 2.0;
        order_size_ = order_size;
        max_position_ = max_position;
        skew_factor_ = skew_factor;
    }
    
    void on_tick(const Tick& tick) override {
        if (tick.symbol != symbol_ || !is_active_) return;
        
        // Cancel existing orders
        if (bid_order_id_ != INVALID_ORDER_ID) {
            cancel_order(bid_order_id_);
            bid_order_id_ = INVALID_ORDER_ID;
        }
        if (ask_order_id_ != INVALID_ORDER_ID) {
            cancel_order(ask_order_id_);
            ask_order_id_ = INVALID_ORDER_ID;
        }
        
        const Position& pos = get_position(symbol_);
        int current_position = static_cast<int>(pos.quantity.to_double());
        
        // Check position limits
        bool can_buy = current_position < max_position_;
        bool can_sell = current_position > -max_position_;
        
        if (!can_buy && !can_sell) return;
        
        // Calculate fair price (mid)
        double mid = tick.mid_price().to_double();
        
        // Skew prices based on inventory
        // If we're long, lower bid/ask to sell inventory
        // If we're short, raise bid/ask to buy inventory
        double inventory_skew = -skew_factor_ * current_position / max_position_;
        mid *= (1.0 + inventory_skew * half_spread_bps_ / 10000.0);
        
        // Calculate bid and ask prices
        double bid_price = mid * (1.0 - half_spread_bps_ / 10000.0);
        double ask_price = mid * (1.0 + half_spread_bps_ / 10000.0);
        
        // Submit orders
        if (can_buy) {
            Order* bid = submit_limit_order(symbol_, Side::Buy, order_size_,
                                           Price::from_double(bid_price));
            if (bid) bid_order_id_ = bid->id;
        }
        
        if (can_sell) {
            Order* ask = submit_limit_order(symbol_, Side::Sell, order_size_,
                                           Price::from_double(ask_price));
            if (ask) ask_order_id_ = ask->id;
        }
    }
    
    void on_fill(const Fill& fill) override {
        if (fill.side == Side::Buy) {
            bid_order_id_ = INVALID_ORDER_ID;
            ++buy_fills_;
        } else {
            ask_order_id_ = INVALID_ORDER_ID;
            ++sell_fills_;
        }
    }
    
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
                   Quantity position_size = Quantity::from_int(100)) {
        symbol_ = symbol;
        lookback_period_ = lookback_period;
        threshold_bps_ = threshold_bps;
        position_size_ = position_size;
    }
    
    void on_tick(const Tick& tick) override {
        if (tick.symbol != symbol_ || !is_active_) return;
        
        double mid = tick.mid_price().to_double();
        prices_.push_back(mid);
        
        if (prices_.size() > static_cast<size_t>(lookback_period_)) {
            prices_.pop_front();
        }
        
        if (prices_.size() < static_cast<size_t>(lookback_period_)) {
            return;
        }
        
        // Calculate momentum (return over lookback)
        double return_pct = (mid - prices_.front()) / prices_.front() * 10000.0;  // in bps
        
        const Position& pos = get_position(symbol_);
        
        if (pos.is_flat()) {
            if (return_pct > threshold_bps_) {
                // Strong upward momentum - go long
                submit_market_order(symbol_, Side::Buy, position_size_);
            } else if (return_pct < -threshold_bps_) {
                // Strong downward momentum - go short
                submit_market_order(symbol_, Side::Sell, position_size_);
            }
        } else if (pos.is_long() && return_pct < 0) {
            // Exit long on momentum reversal
            submit_market_order(symbol_, Side::Sell, pos.abs_quantity());
        } else if (pos.is_short() && return_pct > 0) {
            // Exit short on momentum reversal
            submit_market_order(symbol_, Side::Buy, pos.abs_quantity());
        }
    }

private:
    Symbol symbol_;
    int lookback_period_ = 10;
    double threshold_bps_ = 50.0;
    Quantity position_size_;
    std::deque<double> prices_;
};

}  // namespace backtest
