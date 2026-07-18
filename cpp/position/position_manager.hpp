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
    
    void update_unrealized_pnl(Price current_price);
    
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
    Position& get_position(const Symbol& symbol);

    const Position& get_position(const Symbol& symbol) const;

    // Check if we have a position
    bool has_position(const Symbol& symbol) const;

    // Apply a fill to position
    void apply_fill(const Symbol& symbol, Side side, Quantity fill_qty, Price fill_price);

    // Update price for a symbol
    void update_price(const Symbol& symbol, Price price);

    // Get total portfolio value
    double total_portfolio_value() const;

    // Get total unrealized P&L
    double total_unrealized_pnl() const;

    // Get total realized P&L
    double total_realized_pnl() const;

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
    size_t open_position_count() const;

    // Reset
    void reset();

private:
    std::unordered_map<Symbol, Position, SymbolHash> positions_;
    double cash_;
    double initial_cash_;
};

}  // namespace backtest
