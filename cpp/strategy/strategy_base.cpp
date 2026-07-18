#include "strategy_base.hpp"
#include "execution/execution_simulator.hpp"

namespace backtest {

Order* StrategyBase::submit_market_order(const Symbol& symbol, Side side, Quantity qty) {
    if (!simulator_) return nullptr;
    
    OrderRequest req = OrderRequest::market(symbol, side, qty, name_);
    return simulator_->submit_order(req);
}

Order* StrategyBase::submit_limit_order(const Symbol& symbol, Side side, 
                                        Quantity qty, Price price) {
    if (!simulator_) return nullptr;
    
    OrderRequest req = OrderRequest::limit(symbol, side, qty, price, name_);
    return simulator_->submit_order(req);
}

Order* StrategyBase::submit_ioc_order(const Symbol& symbol, Side side, 
                                      Quantity qty, Price price) {
    if (!simulator_) return nullptr;
    
    OrderRequest req = OrderRequest::ioc(symbol, side, qty, price, name_);
    return simulator_->submit_order(req);
}

bool StrategyBase::cancel_order(OrderId order_id) {
    if (!simulator_) return false;
    return simulator_->cancel_order(order_id);
}

bool StrategyBase::cancel_all_orders() {
    if (!simulator_) return false;
    return simulator_->matching_engine().cancel_all_orders(name_) > 0;
}

const Position& StrategyBase::get_position(const Symbol& symbol) const {
    static Position empty;
    if (!simulator_) return empty;
    return simulator_->position_manager().get_position(symbol);
}

double StrategyBase::get_cash() const {
    if (!simulator_) return 0.0;
    return simulator_->position_manager().cash();
}

double StrategyBase::get_portfolio_value() const {
    if (!simulator_) return 0.0;
    return simulator_->position_manager().total_portfolio_value();
}

const Tick* StrategyBase::get_last_tick(const Symbol& symbol) const {
    if (!simulator_) return nullptr;
    return simulator_->get_last_tick(symbol);
}

Timestamp StrategyBase::current_time() const {
    if (!simulator_) return 0;
    return simulator_->current_time();
}

std::unique_ptr<StrategyBase> StrategyFactory::create(const std::string& name) {
    auto it = creators_.find(name);
    if (it != creators_.end()) {
        return it->second();
    }
    return nullptr;
}

std::vector<std::string> StrategyFactory::available_strategies() const {
    std::vector<std::string> names;
    for (const auto& [name, creator] : creators_) {
        names.push_back(name);
    }
    return names;
}

}  // namespace backtest
