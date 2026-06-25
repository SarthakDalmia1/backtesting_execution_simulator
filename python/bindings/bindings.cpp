#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "core/types.hpp"
#include "core/utils.hpp"
#include "core/timestamp.hpp"
#include "orderbook/order.hpp"
#include "orderbook/orderbook.hpp"
#include "orderbook/matching_engine.hpp"
#include "events/event.hpp"
#include "events/event_queue.hpp"
#include "execution/slippage_model.hpp"
#include "execution/transaction_costs.hpp"
#include "execution/execution_simulator.hpp"
#include "position/position_manager.hpp"
#include "position/pnl_tracker.hpp"
#include "data/market_data_feed.hpp"
#include "strategy/strategy_base.hpp"
#include "strategy/strategies.hpp"
#include "analytics/markout.hpp"

namespace py = pybind11;
using namespace backtest;

// ============================================================================
// Python Strategy Trampoline
// ============================================================================
class PyStrategy : public StrategyBase {
public:
    using StrategyBase::StrategyBase;
    
    void on_start() override {
        PYBIND11_OVERRIDE(void, StrategyBase, on_start);
    }
    
    void on_stop() override {
        PYBIND11_OVERRIDE(void, StrategyBase, on_stop);
    }
    
    void on_tick(const Tick& tick) override {
        PYBIND11_OVERRIDE_PURE(void, StrategyBase, on_tick, tick);
    }
    
    void on_fill(const Fill& fill) override {
        PYBIND11_OVERRIDE(void, StrategyBase, on_fill, fill);
    }
    
    void on_order_reject(OrderId order_id, const std::string& reason) override {
        PYBIND11_OVERRIDE(void, StrategyBase, on_order_reject, order_id, reason);
    }
};

PYBIND11_MODULE(pybacktest, m) {
    m.doc() = R"pbdoc(
        Low-Latency Backtesting & Execution Simulator
        ==============================================
        
        A high-performance tick-level backtesting engine with:
        - Limit order book simulation
        - Market/limit order support
        - Slippage and latency modeling
        - Transaction cost modeling
        - Position and P&L tracking
        - Pluggable strategies
        
        Example:
            >>> import pybacktest as bt
            >>> config = bt.BacktestConfig()
            >>> simulator = bt.ExecutionSimulator(config)
            >>> tick = bt.Tick()
            >>> simulator.add_tick(tick)
    )pbdoc";
    
    // =========================================================================
    // Core Types
    // =========================================================================
    
    py::class_<Price>(m, "Price", "Fixed-point price representation")
        .def(py::init<>())
        .def(py::init<int64_t>())
        .def_static("from_double", &Price::from_double)
        .def("to_double", &Price::to_double)
        .def_readwrite("raw", &Price::raw)
        .def("__repr__", [](const Price& p) {
            return "Price(" + std::to_string(p.to_double()) + ")";
        });
    
    py::class_<Quantity>(m, "Quantity", "Fixed-point quantity representation")
        .def(py::init<>())
        .def(py::init<int64_t>())
        .def_static("from_double", &Quantity::from_double)
        .def_static("from_int", &Quantity::from_int)
        .def("to_double", &Quantity::to_double)
        .def("to_int", &Quantity::to_int)
        .def("is_zero", &Quantity::is_zero)
        .def_readwrite("raw", &Quantity::raw);
    
    py::class_<Symbol>(m, "Symbol", "Trading symbol identifier")
        .def(py::init<>())
        .def(py::init<const char*>())
        .def(py::init<const std::string&>())
        .def("to_string", &Symbol::to_string)
        .def("__repr__", [](const Symbol& s) {
            return "Symbol('" + s.to_string() + "')";
        });
    
    // =========================================================================
    // Enumerations
    // =========================================================================
    
    py::enum_<Side>(m, "Side", "Order side")
        .value("Buy", Side::Buy)
        .value("Sell", Side::Sell);
    
    py::enum_<OrderType>(m, "OrderType", "Order type")
        .value("Market", OrderType::Market)
        .value("Limit", OrderType::Limit)
        .value("StopMarket", OrderType::StopMarket)
        .value("StopLimit", OrderType::StopLimit)
        .value("IOC", OrderType::IOC)
        .value("FOK", OrderType::FOK)
        .value("GTC", OrderType::GTC);
    
    py::enum_<OrderStatus>(m, "OrderStatus", "Order status")
        .value("New", OrderStatus::New)
        .value("PartiallyFilled", OrderStatus::PartiallyFilled)
        .value("Filled", OrderStatus::Filled)
        .value("Cancelled", OrderStatus::Cancelled)
        .value("Rejected", OrderStatus::Rejected)
        .value("Expired", OrderStatus::Expired);
    
    py::enum_<TimeInForce>(m, "TimeInForce", "Time in force")
        .value("Day", TimeInForce::Day)
        .value("GTC", TimeInForce::GTC)
        .value("IOC", TimeInForce::IOC)
        .value("FOK", TimeInForce::FOK);
    
    py::enum_<EventType>(m, "EventType", "Event type")
        .value("MarketData", EventType::MarketData)
        .value("OrderSubmit", EventType::OrderSubmit)
        .value("OrderCancel", EventType::OrderCancel)
        .value("OrderFill", EventType::OrderFill)
        .value("OrderReject", EventType::OrderReject)
        .value("Trade", EventType::Trade)
        .value("PositionUpdate", EventType::PositionUpdate);
    
    // =========================================================================
    // Market Data
    // =========================================================================
    
    py::class_<Tick>(m, "Tick", "Market data tick")
        .def(py::init<>())
        .def_readwrite("timestamp", &Tick::timestamp)
        .def_readwrite("symbol", &Tick::symbol)
        .def_readwrite("bid_price", &Tick::bid_price)
        .def_readwrite("ask_price", &Tick::ask_price)
        .def_readwrite("bid_size", &Tick::bid_size)
        .def_readwrite("ask_size", &Tick::ask_size)
        .def_readwrite("last_price", &Tick::last_price)
        .def_readwrite("last_size", &Tick::last_size)
        .def_readwrite("volume", &Tick::volume)
        .def("mid_price", &Tick::mid_price)
        .def("spread", &Tick::spread);
    
    py::class_<BookLevel>(m, "BookLevel", "Order book price level")
        .def(py::init<>())
        .def_readwrite("price", &BookLevel::price)
        .def_readwrite("quantity", &BookLevel::quantity)
        .def_readwrite("order_count", &BookLevel::order_count);
    
    // =========================================================================
    // Orders
    // =========================================================================
    
    py::class_<Order>(m, "Order", "Order object")
        .def(py::init<>())
        .def_readonly("id", &Order::id)
        .def_readonly("symbol", &Order::symbol)
        .def_readonly("side", &Order::side)
        .def_readonly("type", &Order::type)
        .def_readonly("status", &Order::status)
        .def_readonly("price", &Order::price)
        .def_readonly("quantity", &Order::quantity)
        .def_readonly("filled_quantity", &Order::filled_quantity)
        .def_readonly("remaining_quantity", &Order::remaining_quantity)
        .def_readonly("avg_fill_price", &Order::avg_fill_price)
        .def_readonly("submit_time", &Order::submit_time)
        .def("is_active", &Order::is_active)
        .def("is_complete", &Order::is_complete)
        .def("notional", &Order::notional);
    
    py::class_<OrderRequest>(m, "OrderRequest", "Order submission request")
        .def(py::init<>())
        .def_readwrite("symbol", &OrderRequest::symbol)
        .def_readwrite("side", &OrderRequest::side)
        .def_readwrite("type", &OrderRequest::type)
        .def_readwrite("tif", &OrderRequest::tif)
        .def_readwrite("quantity", &OrderRequest::quantity)
        .def_readwrite("price", &OrderRequest::price)
        .def_readwrite("client_order_id", &OrderRequest::client_order_id)
        .def_readwrite("strategy_id", &OrderRequest::strategy_id)
        .def_static("market", &OrderRequest::market,
                    py::arg("symbol"), py::arg("side"), py::arg("qty"),
                    py::arg("strategy") = "")
        .def_static("limit", &OrderRequest::limit,
                    py::arg("symbol"), py::arg("side"), py::arg("qty"),
                    py::arg("price"), py::arg("strategy") = "");
    
    py::class_<Fill>(m, "Fill", "Order fill information")
        .def(py::init<>())
        .def_readwrite("order_id", &Fill::order_id)
        .def_readwrite("trade_id", &Fill::trade_id)
        .def_readwrite("timestamp", &Fill::timestamp)
        .def_readwrite("fill_price", &Fill::fill_price)
        .def_readwrite("fill_quantity", &Fill::fill_quantity)
        .def_readwrite("side", &Fill::side)
        .def_readwrite("is_maker", &Fill::is_maker);
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    py::class_<TransactionCostConfig>(m, "TransactionCostConfig")
        .def(py::init<>())
        .def_readwrite("maker_fee_bps", &TransactionCostConfig::maker_fee_bps)
        .def_readwrite("taker_fee_bps", &TransactionCostConfig::taker_fee_bps)
        .def_readwrite("fixed_cost", &TransactionCostConfig::fixed_cost)
        .def_readwrite("min_cost", &TransactionCostConfig::min_cost);
    
    py::class_<SlippageConfig>(m, "SlippageConfig")
        .def(py::init<>())
        .def_readwrite("base_slippage_bps", &SlippageConfig::base_slippage_bps)
        .def_readwrite("volume_impact_bps", &SlippageConfig::volume_impact_bps)
        .def_readwrite("use_market_impact", &SlippageConfig::use_market_impact);
    
    py::class_<LatencyConfig>(m, "LatencyConfig")
        .def(py::init<>())
        .def_readwrite("order_submit_latency_ns", &LatencyConfig::order_submit_latency_ns)
        .def_readwrite("order_cancel_latency_ns", &LatencyConfig::order_cancel_latency_ns)
        .def_readwrite("market_data_latency_ns", &LatencyConfig::market_data_latency_ns)
        .def_readwrite("fill_report_latency_ns", &LatencyConfig::fill_report_latency_ns)
        .def_readwrite("deterministic", &LatencyConfig::deterministic);
    
    py::class_<BacktestConfig>(m, "BacktestConfig")
        .def(py::init<>())
        .def_readwrite("start_time", &BacktestConfig::start_time)
        .def_readwrite("end_time", &BacktestConfig::end_time)
        .def_readwrite("initial_capital", &BacktestConfig::initial_capital)
        .def_readwrite("transaction_costs", &BacktestConfig::transaction_costs)
        .def_readwrite("slippage", &BacktestConfig::slippage)
        .def_readwrite("latency", &BacktestConfig::latency)
        .def_readwrite("enable_short_selling", &BacktestConfig::enable_short_selling)
        .def_readwrite("max_position_size", &BacktestConfig::max_position_size);
    
    // =========================================================================
    // Position & P&L
    // =========================================================================
    
    py::class_<Position>(m, "Position", "Position for a symbol")
        .def(py::init<>())
        .def_readonly("symbol", &Position::symbol)
        .def_readonly("quantity", &Position::quantity)
        .def_readonly("avg_entry_price", &Position::avg_entry_price)
        .def_readonly("realized_pnl", &Position::realized_pnl)
        .def_readonly("unrealized_pnl", &Position::unrealized_pnl)
        .def_readonly("market_value", &Position::market_value)
        .def("is_long", &Position::is_long)
        .def("is_short", &Position::is_short)
        .def("is_flat", &Position::is_flat)
        .def("total_pnl", &Position::total_pnl);
    
    py::class_<PositionManager>(m, "PositionManager")
        .def(py::init<double>(), py::arg("initial_cash") = 0.0)
        .def("get_position", 
             static_cast<Position& (PositionManager::*)(const Symbol&)>(&PositionManager::get_position),
             py::return_value_policy::reference)
        .def("has_position", &PositionManager::has_position)
        .def("total_portfolio_value", &PositionManager::total_portfolio_value)
        .def("total_unrealized_pnl", &PositionManager::total_unrealized_pnl)
        .def("total_realized_pnl", &PositionManager::total_realized_pnl)
        .def("total_pnl", &PositionManager::total_pnl)
        .def("cash", &PositionManager::cash)
        .def("open_position_count", &PositionManager::open_position_count);
    
    py::class_<PerformanceStats>(m, "PerformanceStats")
        .def(py::init<>())
        .def_readonly("total_return", &PerformanceStats::total_return)
        .def_readonly("annualized_return", &PerformanceStats::annualized_return)
        .def_readonly("sharpe_ratio", &PerformanceStats::sharpe_ratio)
        .def_readonly("sortino_ratio", &PerformanceStats::sortino_ratio)
        .def_readonly("max_drawdown", &PerformanceStats::max_drawdown)
        .def_readonly("calmar_ratio", &PerformanceStats::calmar_ratio)
        .def_readonly("total_trades", &PerformanceStats::total_trades)
        .def_readonly("win_rate", &PerformanceStats::win_rate)
        .def_readonly("profit_factor", &PerformanceStats::profit_factor)
        .def_readonly("avg_win", &PerformanceStats::avg_win)
        .def_readonly("avg_loss", &PerformanceStats::avg_loss);
    
    py::class_<PnLTracker>(m, "PnLTracker")
        .def(py::init<double>(), py::arg("initial_equity") = 0.0)
        .def("calculate_stats", &PnLTracker::calculate_stats)
        .def("current_equity", &PnLTracker::current_equity)
        .def("high_water_mark", &PnLTracker::high_water_mark)
        .def("total_costs", &PnLTracker::total_costs);
    
    // =========================================================================
    // Order Book & Matching
    // =========================================================================
    
    py::class_<OrderBook>(m, "OrderBook", "Limit order book")
        .def(py::init<const Symbol&>())
        .def("best_bid", &OrderBook::best_bid)
        .def("best_ask", &OrderBook::best_ask)
        .def("best_bid_quantity", &OrderBook::best_bid_quantity)
        .def("best_ask_quantity", &OrderBook::best_ask_quantity)
        .def("spread", &OrderBook::spread)
        .def("mid_price", &OrderBook::mid_price)
        .def("micro_price", &OrderBook::micro_price)
        .def("order_count", &OrderBook::order_count);
    
    py::class_<MatchingEngine>(m, "MatchingEngine", "Order matching engine")
        .def(py::init<>())
        .def("submit_order", &MatchingEngine::submit_order,
             py::return_value_policy::reference)
        .def("cancel_order", &MatchingEngine::cancel_order)
        .def("find_order", &MatchingEngine::find_order,
             py::return_value_policy::reference)
        .def("set_time", &MatchingEngine::set_time)
        .def("get_order_book", &MatchingEngine::get_order_book,
             py::return_value_policy::reference);
    
    // =========================================================================
    // Data Feed
    // =========================================================================
    
    py::class_<VectorDataFeed>(m, "VectorDataFeed", "In-memory tick data feed")
        .def(py::init<>())
        .def("add_tick", &VectorDataFeed::add_tick)
        .def("sort_by_timestamp", &VectorDataFeed::sort_by_timestamp)
        .def("reset", &VectorDataFeed::reset)
        .def("has_data", &VectorDataFeed::has_data)
        .def("total_ticks", &VectorDataFeed::total_ticks);
    
    py::class_<SyntheticTickGenerator>(m, "SyntheticTickGenerator")
        .def(py::init<const Symbol&, double, double, Timestamp, Timestamp, int64_t>(),
             py::arg("symbol"), py::arg("initial_price"), py::arg("volatility"),
             py::arg("start_time"), py::arg("end_time"),
             py::arg("tick_interval_ns") = NANOSECONDS_PER_MILLISECOND * 100)
        .def("reset", &SyntheticTickGenerator::reset)
        .def("has_data", &SyntheticTickGenerator::has_data)
        .def("total_ticks", &SyntheticTickGenerator::total_ticks)
        .def("next_tick", [](SyntheticTickGenerator& self, Tick& tick) {
            return self.next_tick(tick);
        }, py::arg("tick"), "Get next tick, returns true if successful");
    
    // =========================================================================
    // Execution Simulator
    // =========================================================================
    
    py::class_<ExecutionSimulator>(m, "ExecutionSimulator", 
        "Main backtesting engine")
        .def(py::init<const BacktestConfig&>())
        .def("submit_order", &ExecutionSimulator::submit_order,
             py::return_value_policy::reference)
        .def("cancel_order", &ExecutionSimulator::cancel_order)
        .def("add_tick", &ExecutionSimulator::add_tick)
        .def("process_next_event", &ExecutionSimulator::process_next_event)
        .def("run_until", &ExecutionSimulator::run_until)
        .def("run", &ExecutionSimulator::run)
        .def("stop", &ExecutionSimulator::stop)
        .def("total_events_processed", &ExecutionSimulator::total_events_processed)
        .def("current_time", &ExecutionSimulator::current_time)
        .def("position_manager", &ExecutionSimulator::position_manager,
             py::return_value_policy::reference)
        .def("pnl_tracker", &ExecutionSimulator::pnl_tracker,
             py::return_value_policy::reference)
        .def("matching_engine", &ExecutionSimulator::matching_engine,
             py::return_value_policy::reference);
    
    // =========================================================================
    // Strategies
    // =========================================================================
    
    py::class_<StrategyBase, PyStrategy>(m, "Strategy", "Base strategy class")
        .def(py::init<const std::string&>())
        .def("on_start", &StrategyBase::on_start)
        .def("on_stop", &StrategyBase::on_stop)
        .def("on_tick", &StrategyBase::on_tick)
        .def("on_fill", &StrategyBase::on_fill)
        .def("name", &StrategyBase::name)
        .def("is_active", &StrategyBase::is_active)
        .def("set_active", &StrategyBase::set_active);
    
    py::class_<VWAPStrategy, StrategyBase>(m, "VWAPStrategy")
        .def(py::init<>())
        .def("configure", &VWAPStrategy::configure)
        .def("execution_vwap", &VWAPStrategy::execution_vwap)
        .def("market_vwap", &VWAPStrategy::market_vwap)
        .def("slippage_vs_vwap", &VWAPStrategy::slippage_vs_vwap);
    
    py::class_<MeanReversionStrategy, StrategyBase>(m, "MeanReversionStrategy")
        .def(py::init<>())
        .def("configure", &MeanReversionStrategy::configure,
             py::arg("symbol"), py::arg("lookback_period") = 20,
             py::arg("entry_z_score") = 2.0, py::arg("exit_z_score") = 0.5,
             py::arg("position_size") = Quantity::from_int(100))
        .def("current_mean", &MeanReversionStrategy::current_mean)
        .def("current_z_score", &MeanReversionStrategy::current_z_score);
    
    py::class_<MarketMakingStrategy, StrategyBase>(m, "MarketMakingStrategy")
        .def(py::init<>())
        .def("configure", &MarketMakingStrategy::configure,
             py::arg("symbol"), py::arg("spread_bps") = 10.0,
             py::arg("order_size") = Quantity::from_int(100),
             py::arg("max_position") = 1000, py::arg("skew_factor") = 0.1)
        .def("buy_fills", &MarketMakingStrategy::buy_fills)
        .def("sell_fills", &MarketMakingStrategy::sell_fills)
        .def("total_fills", &MarketMakingStrategy::total_fills);
    
    py::class_<MomentumStrategy, StrategyBase>(m, "MomentumStrategy")
        .def(py::init<>())
        .def("configure", &MomentumStrategy::configure,
             py::arg("symbol"), py::arg("lookback_period") = 10,
             py::arg("threshold_bps") = 50.0,
             py::arg("position_size") = Quantity::from_int(100));
    
    // =========================================================================
    // Markout / adverse-selection analyzer
    // =========================================================================

    py::class_<MarkoutConfig>(m, "MarkoutConfig")
        .def(py::init<>())
        .def_readwrite("horizons_ns", &MarkoutConfig::horizons_ns,
                       "Forward horizons in nanoseconds");

    py::class_<MarkoutStats>(m, "MarkoutStats")
        .def_readonly("horizon_ns", &MarkoutStats::horizon_ns)
        .def_readonly("count", &MarkoutStats::count)
        .def_readonly("mean_bps", &MarkoutStats::mean_bps)
        .def_readonly("mean_per_share", &MarkoutStats::mean_per_share)
        .def_readonly("notional_weighted_bps", &MarkoutStats::notional_weighted_bps)
        .def_readonly("win_rate", &MarkoutStats::win_rate)
        .def("__repr__", [](const MarkoutStats& s) {
            return "MarkoutStats(horizon_ns=" + std::to_string(s.horizon_ns) +
                   ", count=" + std::to_string(s.count) +
                   ", mean_bps=" + std::to_string(s.mean_bps) +
                   ", win_rate=" + std::to_string(s.win_rate) + ")";
        });

    py::class_<MarkoutAnalyzer>(m, "MarkoutAnalyzer",
        "Streaming markout / adverse-selection analyzer")
        .def(py::init<MarkoutConfig>(), py::arg("config") = MarkoutConfig())
        .def("record_fill", &MarkoutAnalyzer::record_fill,
             py::arg("fill"), py::arg("mid_at_fill"),
             "Register a fill with the mid prevailing at execution")
        .def("on_mid", &MarkoutAnalyzer::on_mid,
             py::arg("timestamp"), py::arg("mid"),
             "Feed the current mid; resolves any elapsed horizons")
        .def("stats", &MarkoutAnalyzer::stats, "Per-horizon markout statistics")
        .def("effective_spread_bps", &MarkoutAnalyzer::effective_spread_bps)
        .def("unresolved_fills", &MarkoutAnalyzer::unresolved_fills)
        .def("total_fills", &MarkoutAnalyzer::total_fills)
        .def("reset", &MarkoutAnalyzer::reset);

    // =========================================================================
    // Utility Functions
    // =========================================================================
    m.def("to_timestamp", &to_timestamp, 
          "Create timestamp from date/time components",
          py::arg("year"), py::arg("month"), py::arg("day"),
          py::arg("hour") = 0, py::arg("minute") = 0,
          py::arg("second") = 0, py::arg("nano") = 0);
    
    // Constants
    m.attr("NANOSECONDS_PER_SECOND") = NANOSECONDS_PER_SECOND;
    m.attr("NANOSECONDS_PER_MILLISECOND") = NANOSECONDS_PER_MILLISECOND;
    m.attr("NANOSECONDS_PER_MICROSECOND") = NANOSECONDS_PER_MICROSECOND;
}
