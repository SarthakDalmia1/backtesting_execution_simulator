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
    
    ExecutionSimulator(const BacktestConfig& config)
        : config_(config)
        , clock_(config.start_time)
        , slippage_model_(std::make_unique<ZeroSlippageModel>())
        , cost_model_(std::make_unique<ZeroCostModel>())
        , position_manager_(config.initial_capital)
        , pnl_tracker_(config.initial_capital)
        , total_events_processed_(0)
        , is_running_(false) {
        
        // Set up matching engine callbacks
        matching_engine_.set_fill_callback(
            [this](const Fill& fill) { on_fill(fill); });
        matching_engine_.set_trade_callback(
            [this](const Symbol& sym, Price p, Quantity q, Timestamp ts) {
                on_trade(sym, p, q, ts);
            });
    }
    
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
    Order* submit_order(const OrderRequest& request) {
        // Apply latency
        Timestamp submit_time = clock_.now() + config_.latency.order_submit_latency_ns;
        
        // Create order submit event
        OrderSubmitEvent event(submit_time, request);
        event_queue_.push(event);
        
        return nullptr;  // Order created when event is processed
    }
    
    // Cancel order
    bool cancel_order(OrderId order_id) {
        CancelRequest req(order_id);
        Timestamp cancel_time = clock_.now() + config_.latency.order_cancel_latency_ns;
        
        OrderCancelEvent event(cancel_time, req);
        event_queue_.push(event);
        
        return true;
    }
    
    // Add market data tick
    void add_tick(const Tick& tick) {
        // Apply market data latency
        Timestamp adjusted_time = tick.timestamp + config_.latency.market_data_latency_ns;
        
        Tick adjusted_tick = tick;
        adjusted_tick.timestamp = adjusted_time;
        
        MarketDataEvent event(adjusted_tick);
        event_queue_.push(event);
        
        // Update last tick
        last_ticks_[tick.symbol] = tick;
    }
    
    // Process next event
    bool process_next_event() {
        auto event_opt = event_queue_.pop();
        if (!event_opt) {
            return false;
        }
        
        // Update clock
        Timestamp event_time = get_event_timestamp(*event_opt);
        clock_.set_time(event_time);
        matching_engine_.set_time(event_time);
        
        // Process based on event type
        std::visit([this](auto&& event) {
            process_event(event);
        }, *event_opt);
        
        ++total_events_processed_;
        return true;
    }
    
    // Run simulation until time
    void run_until(Timestamp end_time) {
        is_running_ = true;
        
        while (is_running_ && !event_queue_.empty()) {
            Timestamp next_time = event_queue_.next_timestamp();
            if (next_time > end_time) {
                break;
            }
            
            process_next_event();
        }
        
        is_running_ = false;
    }
    
    // Run simulation for all events
    void run() {
        is_running_ = true;
        
        while (is_running_ && !event_queue_.empty()) {
            process_next_event();
        }
        
        is_running_ = false;
    }
    
    // Stop simulation
    void stop() {
        is_running_ = false;
    }
    
    // Get statistics
    size_t total_events_processed() const { return total_events_processed_; }
    Timestamp current_time() const { return clock_.now(); }
    
    // Get last tick for symbol
    const Tick* get_last_tick(const Symbol& symbol) const {
        auto it = last_ticks_.find(symbol);
        return it != last_ticks_.end() ? &it->second : nullptr;
    }

private:
    void process_event(const MarketDataEvent& event) {
        // Update position manager with current prices
        position_manager_.update_price(event.tick.symbol, event.tick.mid_price());
        
        // Notify callback
        if (event_callback_) {
            event_callback_(event);
        }
    }
    
    void process_event(const OrderSubmitEvent& event) {
        // Validate order
        if (!validate_order(event.request)) {
            OrderRejectEvent reject(clock_.now(), 0, "Order validation failed");
            if (event_callback_) {
                event_callback_(reject);
            }
            return;
        }
        
        // Submit to matching engine
        Order* order = matching_engine_.submit_order(event.request);
        
        // Notify callback
        if (event_callback_) {
            event_callback_(event);
        }
        
        (void)order;  // Order tracked by matching engine
    }
    
    void process_event(const OrderCancelEvent& event) {
        bool success = matching_engine_.cancel_order(event.request.order_id);
        
        if (event_callback_) {
            event_callback_(event);
        }
        
        (void)success;
    }
    
    void process_event(const OrderFillEvent& event) {
        if (event_callback_) {
            event_callback_(event);
        }
    }
    
    void process_event(const OrderRejectEvent& event) {
        if (event_callback_) {
            event_callback_(event);
        }
    }
    
    void process_event(const TradeEvent& event) {
        if (event_callback_) {
            event_callback_(event);
        }
    }
    
    void process_event(const PositionUpdateEvent& event) {
        if (event_callback_) {
            event_callback_(event);
        }
    }
    
    void process_event(const SessionStartEvent& event) {
        if (event_callback_) {
            event_callback_(event);
        }
    }
    
    void process_event(const SessionEndEvent& event) {
        if (event_callback_) {
            event_callback_(event);
        }
    }
    
    void process_event(const EndOfDayEvent& event) {
        if (event_callback_) {
            event_callback_(event);
        }
    }
    
    void on_fill(const Fill& fill) {
        // Apply slippage
        Price execution_price = fill.fill_price;
        if (slippage_model_) {
            const Tick* tick = get_last_tick(Symbol(""));  // Would need order symbol
            double adv = tick ? tick->volume : 1000000.0;
            execution_price = slippage_model_->get_execution_price(
                fill.fill_price, fill.fill_quantity, 
                Quantity::from_int(10000), fill.side, 0.02, adv);
        }
        
        // Calculate transaction cost
        double notional = execution_price.to_double() * fill.fill_quantity.to_double();
        double cost = 0.0;
        if (cost_model_) {
            cost = cost_model_->calculate_cost(
                notional, fill.fill_quantity, fill.is_maker, Symbol(""));
        }
        
        // Update position manager
        Order* order = matching_engine_.find_order(fill.order_id);
        if (order) {
            position_manager_.apply_fill(
                order->symbol, fill.side, fill.fill_quantity, execution_price);
            pnl_tracker_.record_trade(
                order->symbol, fill.side, fill.fill_quantity, execution_price, cost);
        }
        
        // Create fill event with adjusted price
        Fill adjusted_fill = fill;
        adjusted_fill.fill_price = execution_price;
        
        OrderFillEvent fill_event(
            clock_.now() + config_.latency.fill_report_latency_ns,
            adjusted_fill, cost);
        
        event_queue_.push(fill_event);
    }
    
    void on_trade(const Symbol& symbol, Price price, Quantity quantity, Timestamp ts) {
        TradeEvent trade(ts, symbol, price, quantity, Side::Buy);
        
        if (event_callback_) {
            event_callback_(trade);
        }
    }
    
    bool validate_order(const OrderRequest& request) {
        // Check position limits
        double notional = request.price.to_double() * request.quantity.to_double();
        
        if (notional > config_.max_position_size) {
            return false;
        }
        
        // Check if short selling is allowed
        if (!config_.enable_short_selling && request.side == Side::Sell) {
            const auto& pos = position_manager_.get_position(request.symbol);
            if (request.quantity > pos.quantity) {
                return false;
            }
        }
        
        // Check margin requirements
        if (config_.enable_margin) {
            double required_margin = notional * config_.margin_requirement;
            if (required_margin > position_manager_.available_cash()) {
                return false;
            }
        }
        
        return true;
    }
    
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
