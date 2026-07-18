#include "strategies.hpp"

namespace backtest {

// ============================================================================
// VWAPStrategy
// ============================================================================
void VWAPStrategy::configure(const Symbol& symbol, Side side, Quantity total_qty,
                             Timestamp start_time, Timestamp end_time, int num_slices) {
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

void VWAPStrategy::on_tick(const Tick& tick) {
    if (tick.symbol != symbol_ || !is_active_) return;

    Timestamp now = current_time();

    // Check if it's time to send next slice
    if (now >= next_slice_time_ && slices_sent_ < num_slices_) {
        execute_slice(tick);
    }

    // Update VWAP calculation
    update_vwap(tick);
}

void VWAPStrategy::on_fill(const Fill& fill) {
    remaining_quantity_ -= fill.fill_quantity;
    filled_notional_ += fill.fill_price.to_double() * fill.fill_quantity.to_double();

    // Track our execution VWAP
    if (remaining_quantity_.is_zero()) {
        is_active_ = false;
    }
}

double VWAPStrategy::slippage_vs_vwap() const {
    double exec_vwap = execution_vwap();
    double mkt_vwap = market_vwap();
    if (mkt_vwap <= 0) return 0.0;

    double slip = (exec_vwap - mkt_vwap) / mkt_vwap;
    return side_ == Side::Buy ? slip : -slip;  // Positive = bad
}

void VWAPStrategy::execute_slice(const Tick& tick) {
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

void VWAPStrategy::update_vwap(const Tick& tick) {
    // Track market VWAP (simplified using last price and implied volume)
    double price = tick.last_price.to_double();
    double volume = tick.last_size.to_double();

    total_pv_ += price * volume;
    total_volume_ += volume;
}

// ============================================================================
// MeanReversionStrategy
// ============================================================================
void MeanReversionStrategy::configure(const Symbol& symbol, int lookback_period,
                                      double entry_z_score, double exit_z_score,
                                      Quantity position_size) {
    symbol_ = symbol;
    lookback_period_ = lookback_period;
    entry_z_score_ = entry_z_score;
    exit_z_score_ = exit_z_score;
    position_size_ = position_size;
}

void MeanReversionStrategy::on_tick(const Tick& tick) {
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

double MeanReversionStrategy::current_mean() const {
    if (prices_.size() < static_cast<size_t>(lookback_period_)) return 0.0;
    return math::sma(prices_, lookback_period_);
}

double MeanReversionStrategy::current_z_score() const {
    if (prices_.size() < static_cast<size_t>(lookback_period_)) return 0.0;
    double mean = math::sma(prices_, lookback_period_);
    double std = math::stddev(prices_);
    if (std < EPSILON) return 0.0;
    return (prices_.back() - mean) / std;
}

// ============================================================================
// MarketMakingStrategy
// ============================================================================
void MarketMakingStrategy::configure(const Symbol& symbol, double spread_bps,
                                     Quantity order_size,
                                     int max_position, double skew_factor) {
    symbol_ = symbol;
    half_spread_bps_ = spread_bps / 2.0;
    order_size_ = order_size;
    max_position_ = max_position;
    skew_factor_ = skew_factor;
}

void MarketMakingStrategy::on_tick(const Tick& tick) {
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

void MarketMakingStrategy::on_fill(const Fill& fill) {
    if (fill.side == Side::Buy) {
        bid_order_id_ = INVALID_ORDER_ID;
        ++buy_fills_;
    } else {
        ask_order_id_ = INVALID_ORDER_ID;
        ++sell_fills_;
    }
}

// ============================================================================
// MomentumStrategy
// ============================================================================
void MomentumStrategy::configure(const Symbol& symbol, int lookback_period,
                                 double threshold_bps, Quantity position_size) {
    symbol_ = symbol;
    lookback_period_ = lookback_period;
    threshold_bps_ = threshold_bps;
    position_size_ = position_size;
}

void MomentumStrategy::on_tick(const Tick& tick) {
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

}  // namespace backtest
