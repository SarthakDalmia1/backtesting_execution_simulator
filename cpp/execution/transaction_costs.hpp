#pragma once

#include "core/types.hpp"
#include <algorithm>

namespace backtest {

// ============================================================================
// Transaction Cost Model Interface
// ============================================================================
class TransactionCostModel {
public:
    virtual ~TransactionCostModel() = default;
    
    // Calculate transaction cost for a fill
    virtual double calculate_cost(
        double notional,
        Quantity quantity,
        bool is_maker,
        const Symbol& symbol
    ) const = 0;
};

// ============================================================================
// Zero Cost Model (no transaction costs)
// ============================================================================
class ZeroCostModel : public TransactionCostModel {
public:
    double calculate_cost(
        double /* notional */,
        Quantity /* quantity */,
        bool /* is_maker */,
        const Symbol& /* symbol */
    ) const override {
        return 0.0;
    }
};

// ============================================================================
// Percentage Cost Model (basis points)
// ============================================================================
class PercentageCostModel : public TransactionCostModel {
public:
    PercentageCostModel(
        double maker_fee_bps = 0.0,
        double taker_fee_bps = 1.0,
        double min_cost = 0.0
    ) : maker_fee_bps_(maker_fee_bps),
        taker_fee_bps_(taker_fee_bps),
        min_cost_(min_cost) {}
    
    double calculate_cost(
        double notional,
        Quantity /* quantity */,
        bool is_maker,
        const Symbol& /* symbol */
    ) const override {
        double fee_bps = is_maker ? maker_fee_bps_ : taker_fee_bps_;
        double cost = notional * fee_bps / 10000.0;
        return std::max(cost, min_cost_);
    }

private:
    double maker_fee_bps_;
    double taker_fee_bps_;
    double min_cost_;
};

// ============================================================================
// Tiered Cost Model (volume-based tiers)
// ============================================================================
class TieredCostModel : public TransactionCostModel {
public:
    struct Tier {
        double volume_threshold;  // Cumulative volume threshold
        double maker_fee_bps;
        double taker_fee_bps;
    };
    
    TieredCostModel(std::vector<Tier> tiers, double min_cost = 0.0)
        : tiers_(std::move(tiers)), min_cost_(min_cost), cumulative_volume_(0.0) {
        // Sort tiers by volume threshold
        std::sort(tiers_.begin(), tiers_.end(),
                  [](const Tier& a, const Tier& b) {
                      return a.volume_threshold < b.volume_threshold;
                  });
    }
    
    double calculate_cost(
        double notional,
        Quantity /* quantity */,
        bool is_maker,
        const Symbol& /* symbol */
    ) const override {
        // Find applicable tier
        const Tier* applicable_tier = &tiers_.back();
        for (const auto& tier : tiers_) {
            if (cumulative_volume_ < tier.volume_threshold) {
                applicable_tier = &tier;
                break;
            }
        }
        
        // Update cumulative volume (mutable in real implementation)
        // cumulative_volume_ += notional;
        
        double fee_bps = is_maker ? applicable_tier->maker_fee_bps 
                                  : applicable_tier->taker_fee_bps;
        double cost = notional * fee_bps / 10000.0;
        return std::max(cost, min_cost_);
    }
    
    void reset_volume() { cumulative_volume_ = 0.0; }
    void add_volume(double vol) { cumulative_volume_ += vol; }

private:
    std::vector<Tier> tiers_;
    double min_cost_;
    mutable double cumulative_volume_;
};

// ============================================================================
// Fixed + Variable Cost Model (exchange-style)
// ============================================================================
class ExchangeCostModel : public TransactionCostModel {
public:
    ExchangeCostModel(
        double exchange_fee_per_share = 0.003,  // SEC/exchange fees
        double clearing_fee_per_share = 0.0001,
        double broker_fee_bps = 0.0,
        double min_cost = 0.01
    ) : exchange_fee_(exchange_fee_per_share),
        clearing_fee_(clearing_fee_per_share),
        broker_fee_bps_(broker_fee_bps),
        min_cost_(min_cost) {}
    
    double calculate_cost(
        double notional,
        Quantity quantity,
        bool /* is_maker */,
        const Symbol& /* symbol */
    ) const override {
        double shares = quantity.to_double();
        
        double cost = 0.0;
        cost += exchange_fee_ * shares;
        cost += clearing_fee_ * shares;
        cost += notional * broker_fee_bps_ / 10000.0;
        
        return std::max(cost, min_cost_);
    }

private:
    double exchange_fee_;
    double clearing_fee_;
    double broker_fee_bps_;
    double min_cost_;
};

// ============================================================================
// Composite Transaction Costs (combines multiple models)
// ============================================================================
class CompositeCostModel : public TransactionCostModel {
public:
    void add_model(std::unique_ptr<TransactionCostModel> model) {
        models_.push_back(std::move(model));
    }
    
    double calculate_cost(
        double notional,
        Quantity quantity,
        bool is_maker,
        const Symbol& symbol
    ) const override {
        double total_cost = 0.0;
        for (const auto& model : models_) {
            total_cost += model->calculate_cost(notional, quantity, is_maker, symbol);
        }
        return total_cost;
    }

private:
    std::vector<std::unique_ptr<TransactionCostModel>> models_;
};

}  // namespace backtest
