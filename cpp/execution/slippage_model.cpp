#include "slippage_model.hpp"

namespace backtest {

Price SlippageModel::get_execution_price(
    Price base_price,
    Quantity order_qty,
    Quantity available_qty,
    Side side,
    double volatility,
    double avg_daily_volume
) const {
    Price slippage = calculate_slippage(
        base_price, order_qty, available_qty, side, volatility, avg_daily_volume);
    return Price(base_price.raw + slippage.raw);
}

Price ZeroSlippageModel::calculate_slippage(
    Price /* base_price */,
    Quantity /* order_qty */,
    Quantity /* available_qty */,
    Side /* side */,
    double /* volatility */,
    double /* avg_daily_volume */
) const {
    return Price::zero();
}

Price FixedSlippageModel::calculate_slippage(
    Price base_price,
    Quantity /* order_qty */,
    Quantity /* available_qty */,
    Side side,
    double /* volatility */,
    double /* avg_daily_volume */
) const {
    int64_t slippage_raw = static_cast<int64_t>(
        base_price.raw * slippage_bps_ / 10000.0);

    // Buy orders slip up, sell orders slip down
    return side == Side::Buy ? Price(slippage_raw) : Price(-slippage_raw);
}

Price VolumeSlippageModel::calculate_slippage(
    Price base_price,
    Quantity order_qty,
    Quantity available_qty,
    Side side,
    double volatility,
    double avg_daily_volume
) const {
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

Price SquareRootImpactModel::calculate_slippage(
    Price base_price,
    Quantity order_qty,
    Quantity /* available_qty */,
    Side side,
    double /* volatility */,
    double avg_daily_volume
) const {
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

}  // namespace backtest
