#include "analytics/markout.hpp"

namespace backtest {

MarkoutAnalyzer::MarkoutAnalyzer(MarkoutConfig config)
    : config_(std::move(config)), accum_(config_.horizons_ns.size()) {}

void MarkoutAnalyzer::reset() {
    pending_ = decltype(pending_)();
    accum_.assign(config_.horizons_ns.size(), Accum());
    num_fills_ = 0;
    eff_spread_sum_bps_ = 0.0;
    eff_spread_count_ = 0;
}

void MarkoutAnalyzer::record_fill(const Fill& fill, Price mid_at_fill) {
    ++num_fills_;

    const double sign = (fill.side == Side::Buy) ? 1.0 : -1.0;
    const double fill_px = fill.fill_price.to_double();
    const double qty = fill.fill_quantity.to_double();
    const double mid = mid_at_fill.to_double();

    // Immediate effective half-spread: signed cost paid relative to mid.
    // Buying above the mid (or selling below) is a positive cost.
    if (mid > EPSILON) {
        eff_spread_sum_bps_ += sign * (fill_px - mid) / mid * 1e4;
        ++eff_spread_count_;
    }

    // Queue one pending evaluation per horizon.
    for (size_t h = 0; h < config_.horizons_ns.size(); ++h) {
        pending_.push(Pending{
            fill.timestamp + config_.horizons_ns[h],
            h, sign, fill_px, qty});
    }
}

void MarkoutAnalyzer::resolve_due(Timestamp ts, double mid) {
    while (!pending_.empty() && pending_.top().resolve_time <= ts) {
        const Pending p = pending_.top();
        pending_.pop();

        if (p.fill_price <= EPSILON) continue;

        const double per_share = p.sign * (mid - p.fill_price);
        const double bps = per_share / p.fill_price * 1e4;

        Accum& a = accum_[p.horizon_index];
        ++a.count;
        a.sum_bps += bps;
        a.sum_per_share += per_share;
        a.sum_w_bps += bps * p.quantity;
        a.sum_weight += p.quantity;
        if (per_share > 0.0) ++a.wins;
    }
}

void MarkoutAnalyzer::on_mid(Timestamp ts, Price mid) {
    resolve_due(ts, mid.to_double());
}

std::vector<MarkoutStats> MarkoutAnalyzer::stats() const {
    std::vector<MarkoutStats> out(config_.horizons_ns.size());
    for (size_t h = 0; h < config_.horizons_ns.size(); ++h) {
        const Accum& a = accum_[h];
        MarkoutStats& s = out[h];
        s.horizon_ns = config_.horizons_ns[h];
        s.count = a.count;
        if (a.count > 0) {
            s.mean_bps = a.sum_bps / static_cast<double>(a.count);
            s.mean_per_share = a.sum_per_share / static_cast<double>(a.count);
            s.win_rate = static_cast<double>(a.wins) / static_cast<double>(a.count);
        }
        if (a.sum_weight > EPSILON) {
            s.notional_weighted_bps = a.sum_w_bps / a.sum_weight;
        }
    }
    return out;
}

double MarkoutAnalyzer::effective_spread_bps() const {
    if (eff_spread_count_ == 0) return 0.0;
    return eff_spread_sum_bps_ / static_cast<double>(eff_spread_count_);
}

} // namespace backtest
