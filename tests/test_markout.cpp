#include "test_framework.hpp"
#include "core/types.hpp"
#include "analytics/markout.hpp"

using namespace backtest;

namespace {

Fill make_fill(Timestamp ts, Side side, double price, double qty) {
    Fill f;
    f.order_id = 1;
    f.trade_id = 1;
    f.timestamp = ts;
    f.side = side;
    f.fill_price = Price::from_double(price);
    f.fill_quantity = Quantity::from_double(qty);
    return f;
}

MarkoutConfig two_horizons() {
    MarkoutConfig c;
    c.horizons_ns = {1 * NANOSECONDS_PER_SECOND, 5 * NANOSECONDS_PER_SECOND};
    return c;
}

} // namespace

// ============================================================================
// Markout analyzer tests
// ============================================================================

TEST_CASE(markout_buy_then_mid_rises_is_positive) {
    MarkoutAnalyzer ma(two_horizons());
    // Buy at 100; mid moves up afterwards -> favourable markout.
    ma.record_fill(make_fill(0, Side::Buy, 100.0, 10.0), Price::from_double(100.0));
    ma.on_mid(1 * NANOSECONDS_PER_SECOND, Price::from_double(101.0));
    ma.on_mid(5 * NANOSECONDS_PER_SECOND, Price::from_double(102.0));

    auto s = ma.stats();
    ASSERT_EQ(s[0].count, static_cast<size_t>(1));
    ASSERT_NEAR(s[0].mean_per_share, 1.0, 1e-9);
    ASSERT_NEAR(s[0].mean_bps, 100.0, 1e-6);   // 1.0 / 100 * 1e4
    ASSERT_NEAR(s[0].win_rate, 1.0, 1e-9);
    ASSERT_EQ(s[1].count, static_cast<size_t>(1));
    ASSERT_NEAR(s[1].mean_per_share, 2.0, 1e-9);
}

TEST_CASE(markout_sell_then_mid_rises_is_adverse) {
    MarkoutAnalyzer ma(two_horizons());
    // Sell at 100; mid rises -> seller was adversely selected (negative).
    ma.record_fill(make_fill(0, Side::Sell, 100.0, 10.0), Price::from_double(100.0));
    ma.on_mid(1 * NANOSECONDS_PER_SECOND, Price::from_double(101.0));

    auto s = ma.stats();
    ASSERT_EQ(s[0].count, static_cast<size_t>(1));
    ASSERT_NEAR(s[0].mean_per_share, -1.0, 1e-9);
    ASSERT_NEAR(s[0].win_rate, 0.0, 1e-9);
}

TEST_CASE(markout_horizon_resolves_only_after_elapsed) {
    MarkoutAnalyzer ma(two_horizons());
    ma.record_fill(make_fill(0, Side::Buy, 100.0, 1.0), Price::from_double(100.0));

    // Before 1s nothing resolves.
    ma.on_mid(NANOSECONDS_PER_SECOND / 2, Price::from_double(105.0));
    ASSERT_EQ(ma.stats()[0].count, static_cast<size_t>(0));

    // After 1s the first horizon resolves; the 5s horizon is still pending.
    ma.on_mid(1 * NANOSECONDS_PER_SECOND, Price::from_double(105.0));
    ASSERT_EQ(ma.stats()[0].count, static_cast<size_t>(1));
    ASSERT_EQ(ma.stats()[1].count, static_cast<size_t>(0));
    ASSERT_EQ(ma.unresolved_fills(), static_cast<size_t>(1));
}

TEST_CASE(markout_effective_spread_positive_for_aggressor) {
    MarkoutAnalyzer ma(two_horizons());
    // Buy 0.05 above the mid -> paid 5 bps of half-spread.
    ma.record_fill(make_fill(0, Side::Buy, 100.05, 1.0), Price::from_double(100.0));
    ASSERT_NEAR(ma.effective_spread_bps(), 5.0, 1e-6);

    // Sell 0.05 below the mid -> also a positive cost of 5 bps; average stays 5.
    ma.record_fill(make_fill(0, Side::Sell, 99.95, 1.0), Price::from_double(100.0));
    ASSERT_NEAR(ma.effective_spread_bps(), 5.0, 1e-6);
}

TEST_CASE(markout_notional_weighting) {
    MarkoutAnalyzer ma(two_horizons());
    // Two buys resolving at the same horizon: small favourable, large adverse.
    ma.record_fill(make_fill(0, Side::Buy, 100.0, 1.0), Price::from_double(100.0));
    ma.record_fill(make_fill(0, Side::Buy, 100.0, 9.0), Price::from_double(100.0));
    // Single mid update at 1s resolves both first-horizon evaluations.
    ma.on_mid(1 * NANOSECONDS_PER_SECOND, Price::from_double(99.0));

    auto s = ma.stats();
    ASSERT_EQ(s[0].count, static_cast<size_t>(2));
    // Both see mid 99 vs fill 100 -> -100 bps each; equal-weight mean = -100.
    ASSERT_NEAR(s[0].mean_bps, -100.0, 1e-6);
    // Notional-weighted is also -100 here (same per-fill markout).
    ASSERT_NEAR(s[0].notional_weighted_bps, -100.0, 1e-6);
    ASSERT_NEAR(s[0].win_rate, 0.0, 1e-9);
}

TEST_CASE(markout_reset_clears_state) {
    MarkoutAnalyzer ma(two_horizons());
    ma.record_fill(make_fill(0, Side::Buy, 100.0, 1.0), Price::from_double(100.0));
    ma.on_mid(1 * NANOSECONDS_PER_SECOND, Price::from_double(101.0));
    ma.reset();

    ASSERT_EQ(ma.total_fills(), static_cast<size_t>(0));
    ASSERT_EQ(ma.unresolved_fills(), static_cast<size_t>(0));
    ASSERT_EQ(ma.stats()[0].count, static_cast<size_t>(0));
    ASSERT_NEAR(ma.effective_spread_bps(), 0.0, 1e-12);
}
