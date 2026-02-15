#pragma once

#include "core/types.hpp"
#include <unordered_map>
#include <cmath>

namespace backtest {

// ============================================================================
// Position for a Single Symbol
// ============================================================================
struct Position {
    Symbol symbol;
    Quantity quantity;          // Positive = long, negative = short
    double avg_entry_price;     // Average entry price
    double realized_pnl;        // Cumulative realized P&L
    double unrealized_pnl;      // Current unrealized P&L
    double market_value;        // Current market value
    Price last_price;           // Last known price
    
    Position() : avg_entry_price(0), realized_pnl(0), 
                 unrealized_pnl(0), market_value(0) {}
    
    explicit Position(const Symbol& sym) : symbol(sym), avg_entry_price(0), 
                                           realized_pnl(0), unrealized_pnl(0), 
                                           market_value(0) {}
    
    bool is_long() const { return quantity.raw > 0; }
    bool is_short() const { return quantity.raw < 0; }
    bool is_flat() const { return quantity.raw == 0; }
    
    Quantity abs_quantity() const {
        return Quantity(std::abs(quantity.raw));
    }
    
    void update_unrealized_pnl(Price current_price) {
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
    
    double total_pnl() const {
        return realized_pnl + unrealized_pnl;
    }
};

// ============================================================================
// Position Manager - Tracks all positions
// ============================================================================
class PositionManager {
public:
    explicit PositionManager(double initial_cash = 0.0)
        : cash_(initial_cash), initial_cash_(initial_cash) {}
    
    // Get position for symbol (creates if doesn't exist)
    Position& get_position(const Symbol& symbol) {
        auto it = positions_.find(symbol);
        if (it == positions_.end()) {
            auto [new_it, inserted] = positions_.emplace(symbol, Position(symbol));
            return new_it->second;
        }
        return it->second;
    }
    
    const Position& get_position(const Symbol& symbol) const {
        static Position empty;
        auto it = positions_.find(symbol);
        return it != positions_.end() ? it->second : empty;
    }
    
    // Check if we have a position
    bool has_position(const Symbol& symbol) const {
        auto it = positions_.find(symbol);
        return it != positions_.end() && !it->second.is_flat();
    }
    
    // Apply a fill to position
    void apply_fill(const Symbol& symbol, Side side, Quantity fill_qty, Price fill_price) {
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
    
    // Update price for a symbol
    void update_price(const Symbol& symbol, Price price) {
        auto it = positions_.find(symbol);
        if (it != positions_.end()) {
            it->second.update_unrealized_pnl(price);
        }
    }
    
    // Get total portfolio value
    double total_portfolio_value() const {
        double value = cash_;
        for (const auto& [sym, pos] : positions_) {
            value += pos.market_value;
        }
        return value;
    }
    
    // Get total unrealized P&L
    double total_unrealized_pnl() const {
        double pnl = 0.0;
        for (const auto& [sym, pos] : positions_) {
            pnl += pos.unrealized_pnl;
        }
        return pnl;
    }
    
    // Get total realized P&L
    double total_realized_pnl() const {
        double pnl = 0.0;
        for (const auto& [sym, pos] : positions_) {
            pnl += pos.realized_pnl;
        }
        return pnl;
    }
    
    // Get total P&L
    double total_pnl() const {
        return total_realized_pnl() + total_unrealized_pnl();
    }
    
    // Cash management
    double cash() const { return cash_; }
    double available_cash() const { return cash_; }  // Could include margin here
    double initial_cash() const { return initial_cash_; }
    
    void add_cash(double amount) { cash_ += amount; }
    void deduct_cash(double amount) { cash_ -= amount; }
    
    // Get all positions
    const std::unordered_map<Symbol, Position, SymbolHash>& positions() const {
        return positions_;
    }
    
    // Get number of open positions
    size_t open_position_count() const {
        size_t count = 0;
        for (const auto& [sym, pos] : positions_) {
            if (!pos.is_flat()) ++count;
        }
        return count;
    }
    
    // Reset
    void reset() {
        positions_.clear();
        cash_ = initial_cash_;
    }

private:
    std::unordered_map<Symbol, Position, SymbolHash> positions_;
    double cash_;
    double initial_cash_;
};

}  // namespace backtest
