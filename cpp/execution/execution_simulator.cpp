#include "execution_simulator.hpp"

namespace backtest {

ExecutionSimulator::ExecutionSimulator(const BacktestConfig& config)
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

Order* ExecutionSimulator::submit_order(const OrderRequest& request) {
    // Apply latency
    Timestamp submit_time = clock_.now() + config_.latency.order_submit_latency_ns;

    // Create order submit event
    OrderSubmitEvent event(submit_time, request);
    event_queue_.push(event);

    return nullptr;  // Order created when event is processed
}

bool ExecutionSimulator::cancel_order(OrderId order_id) {
    CancelRequest req(order_id);
    Timestamp cancel_time = clock_.now() + config_.latency.order_cancel_latency_ns;

    OrderCancelEvent event(cancel_time, req);
    event_queue_.push(event);

    return true;
}

void ExecutionSimulator::add_tick(const Tick& tick) {
    // Apply market data latency
    Timestamp adjusted_time = tick.timestamp + config_.latency.market_data_latency_ns;

    Tick adjusted_tick = tick;
    adjusted_tick.timestamp = adjusted_time;

    MarketDataEvent event(adjusted_tick);
    event_queue_.push(event);

    // Update last tick
    last_ticks_[tick.symbol] = tick;
}

bool ExecutionSimulator::process_next_event() {
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

void ExecutionSimulator::run_until(Timestamp end_time) {
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

void ExecutionSimulator::run() {
    is_running_ = true;

    while (is_running_ && !event_queue_.empty()) {
        process_next_event();
    }

    is_running_ = false;
}

void ExecutionSimulator::process_event(const MarketDataEvent& event) {
    // Update position manager with current prices
    position_manager_.update_price(event.tick.symbol, event.tick.mid_price());

    // Notify callback
    if (event_callback_) {
        event_callback_(event);
    }
}

void ExecutionSimulator::process_event(const OrderSubmitEvent& event) {
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

void ExecutionSimulator::process_event(const OrderCancelEvent& event) {
    bool success = matching_engine_.cancel_order(event.request.order_id);

    if (event_callback_) {
        event_callback_(event);
    }

    (void)success;
}

void ExecutionSimulator::process_event(const OrderFillEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }
}

void ExecutionSimulator::process_event(const OrderRejectEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }
}

void ExecutionSimulator::process_event(const TradeEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }
}

void ExecutionSimulator::process_event(const PositionUpdateEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }
}

void ExecutionSimulator::process_event(const SessionStartEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }
}

void ExecutionSimulator::process_event(const SessionEndEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }
}

void ExecutionSimulator::process_event(const EndOfDayEvent& event) {
    if (event_callback_) {
        event_callback_(event);
    }
}

void ExecutionSimulator::on_fill(const Fill& fill) {
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

void ExecutionSimulator::on_trade(const Symbol& symbol, Price price,
                                  Quantity quantity, Timestamp ts) {
    TradeEvent trade(ts, symbol, price, quantity, Side::Buy);

    if (event_callback_) {
        event_callback_(trade);
    }
}

bool ExecutionSimulator::validate_order(const OrderRequest& request) {
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

}  // namespace backtest
