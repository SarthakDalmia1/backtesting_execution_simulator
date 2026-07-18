#include "transaction_costs.hpp"

namespace backtest {

double ZeroCostModel::calculate_cost(
    double /* notional */,
    Quantity /* quantity */,
    bool /* is_maker */,
    const Symbol& /* symbol */
) const {
    return 0.0;
}

double PercentageCostModel::calculate_cost(
    double notional,
    Quantity /* quantity */,
    bool is_maker,
    const Symbol& /* symbol */
) const {
    double fee_bps = is_maker ? maker_fee_bps_ : taker_fee_bps_;
    double cost = notional * fee_bps / 10000.0;
    return std::max(cost, min_cost_);
}

TieredCostModel::TieredCostModel(std::vector<Tier> tiers, double min_cost)
    : tiers_(std::move(tiers)), min_cost_(min_cost), cumulative_volume_(0.0) {
    // Sort tiers by volume threshold
    std::sort(tiers_.begin(), tiers_.end(),
              [](const Tier& a, const Tier& b) {
                  return a.volume_threshold < b.volume_threshold;
              });
}

double TieredCostModel::calculate_cost(
    double notional,
    Quantity /* quantity */,
    bool is_maker,
    const Symbol& /* symbol */
) const {
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

double ExchangeCostModel::calculate_cost(
    double notional,
    Quantity quantity,
    bool /* is_maker */,
    const Symbol& /* symbol */
) const {
    double shares = quantity.to_double();

    double cost = 0.0;
    cost += exchange_fee_ * shares;
    cost += clearing_fee_ * shares;
    cost += notional * broker_fee_bps_ / 10000.0;

    return std::max(cost, min_cost_);
}

double CompositeCostModel::calculate_cost(
    double notional,
    Quantity quantity,
    bool is_maker,
    const Symbol& symbol
) const {
    double total_cost = 0.0;
    for (const auto& model : models_) {
        total_cost += model->calculate_cost(notional, quantity, is_maker, symbol);
    }
    return total_cost;
}

}  // namespace backtest
