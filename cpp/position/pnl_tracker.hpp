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
                      Price price, double cost);

    // Update equity at mark-to-market
    void mark_to_market(double portfolio_value);

    // Record end of day
    void end_of_day(double portfolio_value, double realized_pnl,
                    double unrealized_pnl, int trade_count);

    // Calculate performance statistics
    PerformanceStats calculate_stats() const;

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
    void reset();

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
