#pragma once

#include "core/types.hpp"
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace backtest {

// ============================================================================
// Trade Record
// ============================================================================
struct TradeRecord {
    Timestamp timestamp;
    Symbol symbol;
    Side side;
    Quantity quantity;
    Price price;
    double notional;
    double transaction_cost;
    double realized_pnl;
    
    TradeRecord() : timestamp(0), side(Side::Buy), notional(0), 
                    transaction_cost(0), realized_pnl(0) {}
};

// ============================================================================
// Daily P&L Record
// ============================================================================
struct DailyPnL {
    Timestamp date;
    double open_equity;
    double close_equity;
    double realized_pnl;
    double unrealized_pnl;
    double total_pnl;
    double return_pct;
    int trade_count;
    double volume_traded;
    double total_costs;
    
    DailyPnL() : date(0), open_equity(0), close_equity(0), realized_pnl(0),
                 unrealized_pnl(0), total_pnl(0), return_pct(0), 
                 trade_count(0), volume_traded(0), total_costs(0) {}
};

// ============================================================================
// Performance Statistics
// ============================================================================
struct PerformanceStats {
    // Returns
    double total_return;
    double annualized_return;
    double daily_return_mean;
    double daily_return_std;
    
    // Risk-adjusted
    double sharpe_ratio;
    double sortino_ratio;
    double calmar_ratio;
    double information_ratio;
    
    // Drawdown
    double max_drawdown;
    double max_drawdown_duration_days;
    double current_drawdown;
    
    // Win/Loss
    int total_trades;
    int winning_trades;
    int losing_trades;
    double win_rate;
    double profit_factor;
    double avg_win;
    double avg_loss;
    double avg_trade_pnl;
    double largest_win;
    double largest_loss;
    
    // Other
    double total_commission;
    double total_slippage;
    double turnover;
    
    PerformanceStats() : total_return(0), annualized_return(0), 
                         daily_return_mean(0), daily_return_std(0),
                         sharpe_ratio(0), sortino_ratio(0), calmar_ratio(0),
                         information_ratio(0), max_drawdown(0),
                         max_drawdown_duration_days(0), current_drawdown(0),
                         total_trades(0), winning_trades(0), losing_trades(0),
                         win_rate(0), profit_factor(0), avg_win(0), avg_loss(0),
                         avg_trade_pnl(0), largest_win(0), largest_loss(0),
                         total_commission(0), total_slippage(0), turnover(0) {}
};

// ============================================================================
// P&L Tracker
// ============================================================================
class PnLTracker {
public:
    explicit PnLTracker(double initial_equity = 0.0)
        : initial_equity_(initial_equity)
        , current_equity_(initial_equity)
        , high_water_mark_(initial_equity)
        , total_realized_pnl_(0)
        , total_costs_(0)
        , total_volume_(0) {}
    
    // Record a trade
    void record_trade(const Symbol& symbol, Side side, Quantity qty, 
                      Price price, double cost) {
        TradeRecord trade;
        trade.timestamp = now_ns();
        trade.symbol = symbol;
        trade.side = side;
        trade.quantity = qty;
        trade.price = price;
        trade.notional = price.to_double() * qty.to_double();
        trade.transaction_cost = cost;
        
        trades_.push_back(trade);
        total_costs_ += cost;
        total_volume_ += trade.notional;
        
        // Deduct costs from equity
        current_equity_ -= cost;
    }
    
    // Update equity at mark-to-market
    void mark_to_market(double portfolio_value) {
        double old_equity = current_equity_;
        current_equity_ = portfolio_value;
        
        // Update high water mark
        if (current_equity_ > high_water_mark_) {
            high_water_mark_ = current_equity_;
        }
        
        // Record daily P&L if it's a new day
        double daily_return = (current_equity_ - old_equity) / old_equity;
        daily_returns_.push_back(daily_return);
    }
    
    // Record end of day
    void end_of_day(double portfolio_value, double realized_pnl, 
                    double unrealized_pnl, int trade_count) {
        DailyPnL daily;
        daily.date = now_ns();
        daily.close_equity = portfolio_value;
        daily.open_equity = daily_pnl_.empty() ? initial_equity_ : 
                            daily_pnl_.back().close_equity;
        daily.realized_pnl = realized_pnl;
        daily.unrealized_pnl = unrealized_pnl;
        daily.total_pnl = realized_pnl + unrealized_pnl;
        daily.return_pct = (daily.close_equity - daily.open_equity) / daily.open_equity;
        daily.trade_count = trade_count;
        
        daily_pnl_.push_back(daily);
        
        // Update high water mark
        if (portfolio_value > high_water_mark_) {
            high_water_mark_ = portfolio_value;
        }
    }
    
    // Calculate performance statistics
    PerformanceStats calculate_stats() const {
        PerformanceStats stats;
        
        if (daily_returns_.empty()) {
            return stats;
        }
        
        // Total return
        stats.total_return = (current_equity_ - initial_equity_) / initial_equity_;
        
        // Annualized return (assuming 252 trading days)
        int days = static_cast<int>(daily_returns_.size());
        if (days > 0) {
            stats.annualized_return = std::pow(1.0 + stats.total_return, 
                                               252.0 / days) - 1.0;
        }
        
        // Mean and std of daily returns
        double sum = std::accumulate(daily_returns_.begin(), daily_returns_.end(), 0.0);
        stats.daily_return_mean = sum / daily_returns_.size();
        
        double sq_sum = 0.0;
        double neg_sq_sum = 0.0;
        for (double r : daily_returns_) {
            sq_sum += (r - stats.daily_return_mean) * (r - stats.daily_return_mean);
            if (r < 0) {
                neg_sq_sum += r * r;
            }
        }
        stats.daily_return_std = std::sqrt(sq_sum / (daily_returns_.size() - 1));
        
        // Sharpe ratio (assuming 0% risk-free rate)
        if (stats.daily_return_std > 0) {
            stats.sharpe_ratio = stats.daily_return_mean / stats.daily_return_std * 
                                 std::sqrt(252.0);
        }
        
        // Sortino ratio
        double downside_std = std::sqrt(neg_sq_sum / daily_returns_.size());
        if (downside_std > 0) {
            stats.sortino_ratio = stats.daily_return_mean / downside_std * 
                                  std::sqrt(252.0);
        }
        
        // Max drawdown
        double peak = initial_equity_;
        double max_dd = 0.0;
        int dd_start = 0;
        int max_dd_duration = 0;
        int current_dd_duration = 0;
        
        double running_equity = initial_equity_;
        for (size_t i = 0; i < daily_returns_.size(); ++i) {
            running_equity *= (1.0 + daily_returns_[i]);
            
            if (running_equity > peak) {
                peak = running_equity;
                if (current_dd_duration > max_dd_duration) {
                    max_dd_duration = current_dd_duration;
                }
                current_dd_duration = 0;
            } else {
                double dd = (peak - running_equity) / peak;
                if (dd > max_dd) {
                    max_dd = dd;
                    dd_start = static_cast<int>(i);
                }
                ++current_dd_duration;
            }
        }
        
        stats.max_drawdown = max_dd;
        stats.max_drawdown_duration_days = max_dd_duration;
        stats.current_drawdown = (high_water_mark_ - current_equity_) / high_water_mark_;
        
        // Calmar ratio
        if (max_dd > 0) {
            stats.calmar_ratio = stats.annualized_return / max_dd;
        }
        
        // Trade statistics
        stats.total_trades = static_cast<int>(trades_.size());
        stats.total_commission = total_costs_;
        stats.turnover = total_volume_ / initial_equity_;
        
        double total_wins = 0.0;
        double total_losses = 0.0;
        
        for (const auto& trade : trades_) {
            if (trade.realized_pnl > 0) {
                ++stats.winning_trades;
                total_wins += trade.realized_pnl;
                stats.largest_win = std::max(stats.largest_win, trade.realized_pnl);
            } else if (trade.realized_pnl < 0) {
                ++stats.losing_trades;
                total_losses += std::abs(trade.realized_pnl);
                stats.largest_loss = std::min(stats.largest_loss, trade.realized_pnl);
            }
        }
        
        if (stats.total_trades > 0) {
            stats.win_rate = static_cast<double>(stats.winning_trades) / stats.total_trades;
        }
        
        if (stats.winning_trades > 0) {
            stats.avg_win = total_wins / stats.winning_trades;
        }
        
        if (stats.losing_trades > 0) {
            stats.avg_loss = total_losses / stats.losing_trades;
        }
        
        if (total_losses > 0) {
            stats.profit_factor = total_wins / total_losses;
        }
        
        double total_trade_pnl = 0.0;
        for (const auto& trade : trades_) {
            total_trade_pnl += trade.realized_pnl;
        }
        
        if (stats.total_trades > 0) {
            stats.avg_trade_pnl = total_trade_pnl / stats.total_trades;
        }
        
        return stats;
    }
    
    // Getters
    double initial_equity() const { return initial_equity_; }
    double current_equity() const { return current_equity_; }
    double high_water_mark() const { return high_water_mark_; }
    double total_realized_pnl() const { return total_realized_pnl_; }
    double total_costs() const { return total_costs_; }
    double total_volume() const { return total_volume_; }
    
    const std::vector<TradeRecord>& trades() const { return trades_; }
    const std::vector<DailyPnL>& daily_pnl() const { return daily_pnl_; }
    const std::deque<double>& daily_returns() const { return daily_returns_; }
    
    // Reset
    void reset() {
        current_equity_ = initial_equity_;
        high_water_mark_ = initial_equity_;
        total_realized_pnl_ = 0;
        total_costs_ = 0;
        total_volume_ = 0;
        trades_.clear();
        daily_pnl_.clear();
        daily_returns_.clear();
    }

private:
    double initial_equity_;
    double current_equity_;
    double high_water_mark_;
    double total_realized_pnl_;
    double total_costs_;
    double total_volume_;
    
    std::vector<TradeRecord> trades_;
    std::vector<DailyPnL> daily_pnl_;
    std::deque<double> daily_returns_;
};

}  // namespace backtest
