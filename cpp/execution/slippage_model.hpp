#pragma once

#include "core/types.hpp"
#include <cmath>

namespace backtest {

// ============================================================================
// Slippage Model Interface
// ============================================================================
class SlippageModel {
public:
    virtual ~SlippageModel() = default;
    
    // Calculate slippage for an order
    virtual Price calculate_slippage(
        Price base_price,
        Quantity order_qty,
        Quantity available_qty,
        Side side,
        double volatility = 0.0,
        double avg_daily_volume = 0.0
    ) const = 0;
    
    // Get effective execution price after slippage
    Price get_execution_price(
        Price base_price,
        Quantity order_qty,
        Quantity available_qty,
        Side side,
        double volatility = 0.0,
        double avg_daily_volume = 0.0
    ) const {
        Price slippage = calculate_slippage(
            base_price, order_qty, available_qty, side, volatility, avg_daily_volume);
        return Price(base_price.raw + slippage.raw);
    }
};

// ============================================================================
// Zero Slippage Model (perfect execution)
// ============================================================================
class ZeroSlippageModel : public SlippageModel {
public:
    Price calculate_slippage(
        Price /* base_price */,
        Quantity /* order_qty */,
        Quantity /* available_qty */,
        Side /* side */,
        double /* volatility */,
        double /* avg_daily_volume */
    ) const override {
        return Price::zero();
    }
};

// ============================================================================
// Fixed Slippage Model (constant basis points)
// ============================================================================
class FixedSlippageModel : public SlippageModel {
public:
    explicit FixedSlippageModel(double slippage_bps = 1.0) 
        : slippage_bps_(slippage_bps) {}
    
    Price calculate_slippage(
        Price base_price,
        Quantity /* order_qty */,
        Quantity /* available_qty */,
        Side side,
        double /* volatility */,
        double /* avg_daily_volume */
    ) const override {
        int64_t slippage_raw = static_cast<int64_t>(
            base_price.raw * slippage_bps_ / 10000.0);
        
        // Buy orders slip up, sell orders slip down
        return side == Side::Buy ? Price(slippage_raw) : Price(-slippage_raw);
    }

private:
    double slippage_bps_;
};

// ============================================================================
// Volume-Based Slippage Model
// ============================================================================
// Models market impact based on participation rate
class VolumeSlippageModel : public SlippageModel {
public:
    VolumeSlippageModel(
        double base_slippage_bps = 0.5,
        double volume_impact_factor = 0.1,
        double volatility_factor = 0.5
    ) : base_slippage_bps_(base_slippage_bps),
        volume_impact_factor_(volume_impact_factor),
        volatility_factor_(volatility_factor) {}
    
    Price calculate_slippage(
        Price base_price,
        Quantity order_qty,
        Quantity available_qty,
        Side side,
        double volatility,
        double avg_daily_volume
    ) const override {
        double slippage_bps = base_slippage_bps_;
        
        // Volume impact: sqrt(participation_rate) * impact_factor
        if (avg_daily_volume > 0) {
            double participation = order_qty.to_double() / avg_daily_volume;
            slippage_bps += volume_impact_factor_ * std::sqrt(participation) * 10000.0;
        }
        
        // Liquidity impact: if order is larger than available
        if (available_qty.raw > 0 && order_qty > available_qty) {
            double liquidity_ratio = order_qty.to_double() / available_qty.to_double();
            slippage_bps += (liquidity_ratio - 1.0) * 5.0;  // 5 bps per 100% of available
        }
        
        // Volatility impact
        slippage_bps *= (1.0 + volatility_factor_ * volatility);
        
        int64_t slippage_raw = static_cast<int64_t>(
            base_price.raw * slippage_bps / 10000.0);
        
        return side == Side::Buy ? Price(slippage_raw) : Price(-slippage_raw);
    }

private:
    double base_slippage_bps_;
    double volume_impact_factor_;
    double volatility_factor_;
};

// ============================================================================
// Square Root Market Impact Model (Almgren-Chriss inspired)
// ============================================================================
class SquareRootImpactModel : public SlippageModel {
public:
    SquareRootImpactModel(
        double sigma = 0.02,      // Daily volatility
        double eta = 0.01,        // Permanent impact coefficient
        double gamma = 0.1        // Temporary impact coefficient
    ) : sigma_(sigma), eta_(eta), gamma_(gamma) {}
    
    Price calculate_slippage(
        Price base_price,
        Quantity order_qty,
        Quantity /* available_qty */,
        Side side,
        double /* volatility */,
        double avg_daily_volume
    ) const override {
        if (avg_daily_volume <= 0) {
            // Fallback to simple model
            return Price(static_cast<int64_t>(base_price.raw * 0.0001));
        }
        
        // Participation rate
        double x = order_qty.to_double() / avg_daily_volume;
        
        // Temporary impact: gamma * sigma * sqrt(x)
        double temporary_impact = gamma_ * sigma_ * std::sqrt(x);
        
        // Permanent impact: eta * x
        double permanent_impact = eta_ * x;
        
        // Total impact
        double total_impact = temporary_impact + permanent_impact;
        
        int64_t slippage_raw = static_cast<int64_t>(
            base_price.raw * total_impact);
        
        return side == Side::Buy ? Price(slippage_raw) : Price(-slippage_raw);
    }

private:
    double sigma_;
    double eta_;
    double gamma_;
};

}  // namespace backtest
