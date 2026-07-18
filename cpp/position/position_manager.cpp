#include "position_manager.hpp"

#include <algorithm>

namespace backtest {

void Position::update_unrealized_pnl(Price current_price) {
    last_price = current_price;
    market_value = current_price.to_double() * quantity.to_double();

    if (!is_flat()) {
        double current_value = current_price.to_double() * quantity.to_double();
        double entry_value = avg_entry_price * quantity.to_double();
        unrealized_pnl = current_value - entry_value;
    } else {
        unrealized_pnl = 0.0;
    }
}

Position& PositionManager::get_position(const Symbol& symbol) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        auto [new_it, inserted] = positions_.emplace(symbol, Position(symbol));
        return new_it->second;
    }
    return it->second;
}

const Position& PositionManager::get_position(const Symbol& symbol) const {
    static Position empty;
    auto it = positions_.find(symbol);
    return it != positions_.end() ? it->second : empty;
}

bool PositionManager::has_position(const Symbol& symbol) const {
    auto it = positions_.find(symbol);
    return it != positions_.end() && !it->second.is_flat();
}

void PositionManager::apply_fill(const Symbol& symbol, Side side,
                                 Quantity fill_qty, Price fill_price) {
    Position& pos = get_position(symbol);

    Quantity signed_qty = (side == Side::Buy) ? fill_qty :
                          Quantity(-fill_qty.raw);

    double fill_notional = fill_price.to_double() * fill_qty.to_double();

    // Check if this is reducing position (realize P&L) or adding
    bool is_reducing = (pos.is_long() && side == Side::Sell) ||
                      (pos.is_short() && side == Side::Buy);

    if (is_reducing) {
        // Calculate realized P&L
        double realized = 0.0;

        if (pos.is_long()) {
            // Closing long position
            realized = (fill_price.to_double() - pos.avg_entry_price) *
                       std::min(fill_qty.to_double(), pos.quantity.to_double());
        } else {
            // Closing short position
            realized = (pos.avg_entry_price - fill_price.to_double()) *
                       std::min(fill_qty.to_double(), -pos.quantity.to_double());
        }

        pos.realized_pnl += realized;

        // Check if we're flipping sides
        Quantity new_qty = pos.quantity + signed_qty;

        if ((pos.is_long() && new_qty.raw <= 0) ||
            (pos.is_short() && new_qty.raw >= 0)) {
            // Position flipped or closed
            if (new_qty.raw != 0) {
                // Flipped - new avg entry is fill price
                pos.avg_entry_price = fill_price.to_double();
            } else {
                // Closed
                pos.avg_entry_price = 0.0;
            }
        }

        pos.quantity = new_qty;
    } else {
        // Adding to position - update average entry price
        double old_notional = pos.avg_entry_price * pos.abs_quantity().to_double();
        double new_notional = old_notional + fill_notional;

        pos.quantity = pos.quantity + signed_qty;

        if (!pos.is_flat()) {
            pos.avg_entry_price = new_notional / pos.abs_quantity().to_double();
        }
    }

    // Update cash
    if (side == Side::Buy) {
        cash_ -= fill_notional;
    } else {
        cash_ += fill_notional;
    }

    // Update unrealized P&L
    pos.update_unrealized_pnl(fill_price);
}

void PositionManager::update_price(const Symbol& symbol, Price price) {
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        it->second.update_unrealized_pnl(price);
    }
}

double PositionManager::total_portfolio_value() const {
    double value = cash_;
    for (const auto& [sym, pos] : positions_) {
        value += pos.market_value;
    }
    return value;
}

double PositionManager::total_unrealized_pnl() const {
    double pnl = 0.0;
    for (const auto& [sym, pos] : positions_) {
        pnl += pos.unrealized_pnl;
    }
    return pnl;
}

double PositionManager::total_realized_pnl() const {
    double pnl = 0.0;
    for (const auto& [sym, pos] : positions_) {
        pnl += pos.realized_pnl;
    }
    return pnl;
}

size_t PositionManager::open_position_count() const {
    size_t count = 0;
    for (const auto& [sym, pos] : positions_) {
        if (!pos.is_flat()) ++count;
    }
    return count;
}

void PositionManager::reset() {
    positions_.clear();
    cash_ = initial_cash_;
}

}  // namespace backtest
