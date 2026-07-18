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
    ) const;
};

// ============================================================================
// Zero Slippage Model (perfect execution)
// ============================================================================
class ZeroSlippageModel : public SlippageModel {
public:
    Price calculate_slippage(
        Price base_price,
        Quantity order_qty,
        Quantity available_qty,
        Side side,
        double volatility,
        double avg_daily_volume
    ) const override;
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
        Quantity order_qty,
        Quantity available_qty,
        Side side,
        double volatility,
        double avg_daily_volume
    ) const override;

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
    ) const override;

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
        Quantity available_qty,
        Side side,
        double volatility,
        double avg_daily_volume
    ) const override;

private:
    double sigma_;
    double eta_;
    double gamma_;
};

}  // namespace backtest
