#pragma once

#include "core/types.hpp"
#include "events/event.hpp"
#include "orderbook/order.hpp"
#include "position/position_manager.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace backtest {

// Forward declaration
class ExecutionSimulator;

// ============================================================================
// Strategy Base Class
// ============================================================================
// All strategies must inherit from this class
class StrategyBase {
public:
    explicit StrategyBase(const std::string& name) 
        : name_(name), simulator_(nullptr), is_active_(true) {}
    
    virtual ~StrategyBase() = default;
    
    // Lifecycle methods
    virtual void on_start() {}
    virtual void on_stop() {}
    
    // Event handlers
    virtual void on_tick(const Tick& tick) = 0;
    virtual void on_fill(const Fill& fill) {}
    virtual void on_order_reject(OrderId order_id, const std::string& reason) {}
    virtual void on_trade(const Symbol& symbol, Price price, Quantity qty) {}
    virtual void on_position_update(const Position& position) {}
    virtual void on_session_start(Timestamp start, Timestamp end) {}
    virtual void on_session_end() {}
    
    // Configuration
    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }
    
    bool is_active() const { return is_active_; }
    void set_active(bool active) { is_active_ = active; }
    
    // Set simulator (called by framework)
    void set_simulator(ExecutionSimulator* sim) { simulator_ = sim; }
    
protected:
    // Order submission helpers (implemented in cpp file)
    Order* submit_market_order(const Symbol& symbol, Side side, Quantity qty);
    Order* submit_limit_order(const Symbol& symbol, Side side, Quantity qty, Price price);
    Order* submit_ioc_order(const Symbol& symbol, Side side, Quantity qty, Price price);
    bool cancel_order(OrderId order_id);
    bool cancel_all_orders();
    
    // Position access
    const Position& get_position(const Symbol& symbol) const;
    double get_cash() const;
    double get_portfolio_value() const;
    
    // Market data access
    const Tick* get_last_tick(const Symbol& symbol) const;
    
    // Time access
    Timestamp current_time() const;
    
    std::string name_;
    ExecutionSimulator* simulator_;
    bool is_active_;
};

// ============================================================================
// Strategy Factory
// ============================================================================
class StrategyFactory {
public:
    using Creator = std::function<std::unique_ptr<StrategyBase>()>;
    
    static StrategyFactory& instance() {
        static StrategyFactory factory;
        return factory;
    }
    
    void register_strategy(const std::string& name, Creator creator) {
        creators_[name] = std::move(creator);
    }
    
    std::unique_ptr<StrategyBase> create(const std::string& name);

    std::vector<std::string> available_strategies() const;

private:
    std::unordered_map<std::string, Creator> creators_;
};

// Helper macro for strategy registration
#define REGISTER_STRATEGY(StrategyClass) \
    namespace { \
        static bool _registered_##StrategyClass = []() { \
            StrategyFactory::instance().register_strategy( \
                #StrategyClass, \
                []() { return std::make_unique<StrategyClass>(); } \
            ); \
            return true; \
        }(); \
    }

}  // namespace backtest
