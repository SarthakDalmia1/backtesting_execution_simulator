#pragma once

#include "core/types.hpp"
#include "core/timestamp.hpp"
#include "events/event.hpp"
#include "events/event_queue.hpp"
#include "orderbook/order.hpp"
#include "orderbook/matching_engine.hpp"
#include "execution/slippage_model.hpp"
#include "execution/transaction_costs.hpp"
#include "position/position_manager.hpp"
#include "position/pnl_tracker.hpp"
#include "data/market_data_feed.hpp"
#include <memory>
#include <functional>
#include <unordered_map>

namespace backtest {

// Forward declarations
class StrategyBase;

// ============================================================================
// Execution Simulator - Main Engine
// ============================================================================
class ExecutionSimulator {
public:
    using EventCallback = std::function<void(const Event&)>;
    
    ExecutionSimulator(const BacktestConfig& config);
    
    // Set models
    void set_slippage_model(std::unique_ptr<SlippageModel> model) {
        slippage_model_ = std::move(model);
    }
    
    void set_cost_model(std::unique_ptr<TransactionCostModel> model) {
        cost_model_ = std::move(model);
    }
    
    // Register event callbacks
    void set_event_callback(EventCallback cb) {
        event_callback_ = std::move(cb);
    }
    
    // Access components
    SimulatedClock& clock() { return clock_; }
    MatchingEngine& matching_engine() { return matching_engine_; }
    PositionManager& position_manager() { return position_manager_; }
    PnLTracker& pnl_tracker() { return pnl_tracker_; }
    EventQueue& event_queue() { return event_queue_; }
    
    // Submit order
    Order* submit_order(const OrderRequest& request);

    // Cancel order
    bool cancel_order(OrderId order_id);

    // Add market data tick
    void add_tick(const Tick& tick);

    // Process next event
    bool process_next_event();

    // Run simulation until time
    void run_until(Timestamp end_time);

    // Run simulation for all events
    void run();

    // Stop simulation
    void stop() { is_running_ = false; }

    // Get statistics
    size_t total_events_processed() const { return total_events_processed_; }
    Timestamp current_time() const { return clock_.now(); }
    
    // Get last tick for symbol
    const Tick* get_last_tick(const Symbol& symbol) const {
        auto it = last_ticks_.find(symbol);
        return it != last_ticks_.end() ? &it->second : nullptr;
    }

private:
    void process_event(const MarketDataEvent& event);
    void process_event(const OrderSubmitEvent& event);
    void process_event(const OrderCancelEvent& event);
    void process_event(const OrderFillEvent& event);
    void process_event(const OrderRejectEvent& event);
    void process_event(const TradeEvent& event);
    void process_event(const PositionUpdateEvent& event);
    void process_event(const SessionStartEvent& event);
    void process_event(const SessionEndEvent& event);
    void process_event(const EndOfDayEvent& event);

    void on_fill(const Fill& fill);
    void on_trade(const Symbol& symbol, Price price, Quantity quantity, Timestamp ts);

    bool validate_order(const OrderRequest& request);

    BacktestConfig config_;
    SimulatedClock clock_;
    EventQueue event_queue_;
    MatchingEngine matching_engine_;
    std::unique_ptr<SlippageModel> slippage_model_;
    std::unique_ptr<TransactionCostModel> cost_model_;
    PositionManager position_manager_;
    PnLTracker pnl_tracker_;
    EventCallback event_callback_;
    std::unordered_map<Symbol, Tick, SymbolHash> last_ticks_;
    size_t total_events_processed_;
    bool is_running_;
};

}  // namespace backtest
