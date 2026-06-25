#pragma once

#include "core/types.hpp"
#include <vector>
#include <queue>

namespace backtest {

// ============================================================================
// Markout / adverse-selection analyzer
// ============================================================================
//
// A backtest that only reports realised P&L hides *why* a strategy makes or
// loses money. The single most important diagnostic for any passive / market-
// making style is the markout: after I trade, which way does the mid move?
//
//   - Buy a fill, mid drifts up   -> positive markout (good, I bought cheap)
//   - Buy a fill, mid drifts down -> negative markout (adverse selection: an
//                                    informed counterparty picked me off)
//
// For each fill we measure the signed mid move at a set of forward horizons
// (e.g. 1s, 5s, 30s, 60s). Persistently negative short-horizon markout is the
// classic signature of being adversely selected; recovering markout at longer
// horizons points to transient impact instead.
//
// Design: streaming and bounded-memory. Fills register a pending evaluation per
// horizon on a min-heap keyed by resolution time. As mid updates arrive in
// timestamp order, every pending evaluation whose horizon has elapsed is
// resolved against the first mid observed at/after the horizon and then
// discarded. Nothing is stored for the full history, so it scales to a full day
// of ticks the same way the rest of the engine does.

struct MarkoutConfig {
    // Forward horizons (nanoseconds) at which to measure the mid move.
    std::vector<Timestamp> horizons_ns = {
        1 * NANOSECONDS_PER_SECOND,
        5 * NANOSECONDS_PER_SECOND,
        30 * NANOSECONDS_PER_SECOND,
        60 * NANOSECONDS_PER_SECOND,
    };
};

struct MarkoutStats {
    Timestamp horizon_ns = 0;
    size_t count = 0;              // fills resolved at this horizon
    double mean_bps = 0.0;         // mean signed markout, basis points of fill price
    double mean_per_share = 0.0;   // mean signed markout in price units
    double notional_weighted_bps = 0.0;  // markout weighted by traded quantity
    double win_rate = 0.0;         // fraction of fills with positive markout
};

class MarkoutAnalyzer {
public:
    explicit MarkoutAnalyzer(MarkoutConfig config = MarkoutConfig());

    // Register a fill together with the mid prevailing at execution time. The
    // mid-at-fill is used for the immediate effective-spread metric and is not
    // required to equal the fill price.
    void record_fill(const Fill& fill, Price mid_at_fill);

    // Feed the current mid as the market evolves (timestamps must be
    // non-decreasing). Resolves any pending markouts whose horizon has elapsed.
    void on_mid(Timestamp ts, Price mid);

    // Per-horizon results computed from fills resolved so far.
    std::vector<MarkoutStats> stats() const;

    // Mean effective half-spread paid versus the mid at execution, in bps.
    // Positive means the strategy paid spread (typical for aggressors);
    // negative means it earned spread (typical for passive makers).
    double effective_spread_bps() const;

    // Fills whose longest horizon never elapsed before the data ended.
    size_t unresolved_fills() const { return pending_.size(); }

    size_t total_fills() const { return num_fills_; }

    void reset();

private:
    struct Pending {
        Timestamp resolve_time;
        size_t horizon_index;
        double sign;        // +1 for buys, -1 for sells
        double fill_price;
        double quantity;
        // Min-heap on resolve_time.
        bool operator>(const Pending& o) const { return resolve_time > o.resolve_time; }
    };

    struct Accum {
        size_t count = 0;
        double sum_bps = 0.0;
        double sum_per_share = 0.0;
        double sum_w_bps = 0.0;     // notional-weighted numerator
        double sum_weight = 0.0;    // sum of quantities
        size_t wins = 0;
    };

    MarkoutConfig config_;
    std::priority_queue<Pending, std::vector<Pending>, std::greater<Pending>> pending_;
    std::vector<Accum> accum_;
    size_t num_fills_ = 0;

    // Immediate effective-spread accumulators.
    double eff_spread_sum_bps_ = 0.0;
    size_t eff_spread_count_ = 0;

    void resolve_due(Timestamp ts, double mid);
};

} // namespace backtest
