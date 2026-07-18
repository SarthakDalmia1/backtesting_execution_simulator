# Low-Latency Backtesting & Execution Simulator

A high-performance, tick-level backtesting engine for quantitative trading strategies, built with C++20 and Python bindings.

![C++](https://img.shields.io/badge/C++-20-blue.svg)
![Python](https://img.shields.io/badge/Python-3.8+-green.svg)
![License](https://img.shields.io/badge/License-MIT-yellow.svg)

## Features

- **Limit Order Book Simulation**: Full price-time priority matching engine
- **Order Types**: Market, Limit, IOC (Immediate or Cancel), FOK (Fill or Kill), GTC
- **Slippage & Latency Modeling**: Configurable market impact and network latency
- **Transaction Costs**: Maker/taker fees, tiered pricing, exchange costs
- **Position & P&L Tracking**: Real-time position management with performance analytics
- **Event-Driven Architecture**: Time-ordered event queue with nanosecond precision
- **Pluggable Strategies**: VWAP, Mean Reversion, Market Making, Momentum
- **Python Bindings**: Full pybind11 integration for strategy development
- **Memory Pooling**: Pre-allocated memory for zero-allocation hot paths
- **Lock-Free Queues**: Optional SPSC queues for multi-threaded scenarios

## Performance Target

**Replay 1 day of ticks in < 1 second**

- ~2.3 million ticks (10ms intervals over 6.5 trading hours)
- Achieves 2M+ ticks/second throughput
- Nanosecond timestamp precision

## Table of Contents

1. [Architecture](#architecture)
2. [Core Components](#core-components)
3. [Building](#building)
4. [Usage](#usage)
5. [Strategy Development](#strategy-development)
6. [Performance Considerations](#performance-considerations)
7. [API Reference](#api-reference)

---

## Architecture

```
backtesting_execution_simulator/
├── cpp/                            # each module is a .hpp/.cpp pair:
│   │                               # declarations + trivial accessors in the
│   │                               # header, implementations in the source
│   ├── core/
│   │   ├── types.hpp/.cpp          # Fixed-point prices, timestamps, symbols
│   │   ├── utils.hpp/.cpp          # Mathematical utilities
│   │   ├── memory_pool.hpp         # Zero-allocation memory management (templates)
│   │   └── timestamp.hpp/.cpp      # Nanosecond time handling
│   ├── orderbook/
│   │   ├── order.hpp/.cpp          # Order structure
│   │   ├── orderbook.hpp/.cpp      # Limit order book implementation
│   │   └── matching_engine.hpp/.cpp # Price-time priority matching
│   ├── events/
│   │   ├── event.hpp/.cpp          # Event types (MarketData, Fill, etc.)
│   │   └── event_queue.hpp/.cpp    # Priority queue + lock-free SPSC
│   ├── execution/
│   │   ├── slippage_model.hpp/.cpp      # Market impact models
│   │   ├── transaction_costs.hpp/.cpp   # Fee structures
│   │   └── execution_simulator.hpp/.cpp # Main simulation engine
│   ├── position/
│   │   ├── position_manager.hpp/.cpp    # Position tracking
│   │   └── pnl_tracker.hpp/.cpp         # Performance analytics
│   ├── analytics/
│   │   └── markout.hpp/.cpp        # Markout / adverse-selection analyzer
│   ├── data/
│   │   ├── market_data_feed.hpp/.cpp    # Data feed interfaces
│   │   └── tick_reader.hpp/.cpp         # CSV/binary tick readers
│   ├── strategy/
│   │   ├── strategy_base.hpp/.cpp       # Base strategy class
│   │   └── strategies.hpp/.cpp          # Built-in strategies
│   └── benchmarks/
│       └── benchmark_main.cpp
├── python/
│   └── bindings/
│       └── bindings.cpp        # pybind11 bindings
├── tests/
│   ├── test_orderbook.cpp
│   ├── test_matching_engine.cpp
│   ├── test_event_queue.cpp
│   ├── test_execution.cpp
│   ├── test_position.cpp
│   ├── test_strategies.cpp
│   └── test_markout.cpp
└── examples/
    └── demo.py
```

---

## Core Components

### 1. Order Book & Matching Engine

Price-time priority matching with O(1) best bid/ask access:

```cpp
// Add limit order
OrderRequest req = OrderRequest::limit(
    Symbol("AAPL"), Side::Buy, 
    Quantity::from_int(100), Price::from_double(150.0)
);
Order* order = engine.submit_order(req);

// Check book state
Price spread = book.spread();
Price mid = book.mid_price();
Price micro = book.micro_price();  // Volume-weighted mid
```

### 2. Event-Driven Simulation

All actions (ticks, orders, fills) are events processed in timestamp order:

```cpp
// Events are automatically ordered by timestamp
simulator.add_tick(tick);  // Market data event
simulator.submit_order(req);  // Order event (with latency)

// Process events in time order
while (simulator.process_next_event()) {
    // Events trigger callbacks
}
```

### 3. Slippage & Market Impact Models

Multiple slippage models available:

| Model | Description |
|-------|-------------|
| `ZeroSlippageModel` | Perfect execution (testing) |
| `FixedSlippageModel` | Constant basis points |
| `VolumeSlippageModel` | Participation-based impact |
| `SquareRootImpactModel` | Almgren-Chriss inspired |

```cpp
// Configure volume-based slippage
auto slippage = std::make_unique<VolumeSlippageModel>(
    0.5,   // Base slippage (bps)
    0.1,   // Volume impact factor
    0.5    // Volatility factor
);
simulator.set_slippage_model(std::move(slippage));
```

### 4. Transaction Costs

```cpp
// Tiered exchange-style costs
auto costs = std::make_unique<PercentageCostModel>(
    0.0,   // Maker fee (bps) - rebate possible with negative
    1.0,   // Taker fee (bps)
    0.01   // Minimum cost
);
simulator.set_cost_model(std::move(costs));
```

### 5. Position & P&L Tracking

Real-time position management with performance analytics:

```cpp
// Check position
const Position& pos = pm.get_position(symbol);
double unrealized = pos.unrealized_pnl;
double realized = pos.realized_pnl;

// Performance stats
PerformanceStats stats = pnl_tracker.calculate_stats();
double sharpe = stats.sharpe_ratio;
double max_dd = stats.max_drawdown;
double win_rate = stats.win_rate;
```

---

## Building

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 11+)
- CMake 3.16+
- pybind11 (for Python bindings)

### Build Commands

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
./test_runner

# Run benchmarks
./benchmarks
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_PYTHON_BINDINGS` | ON | Build pybind11 Python module |
| `BUILD_BENCHMARKS` | ON | Build performance benchmarks |
| `BUILD_TESTS` | ON | Build test suite |

---

## Usage

### C++ Example

```cpp
#include "execution/execution_simulator.hpp"
#include "data/market_data_feed.hpp"
#include "strategy/strategies.hpp"

using namespace backtest;

int main() {
    // Configure backtest
    BacktestConfig config;
    config.initial_capital = 1'000'000.0;
    config.start_time = to_timestamp(2024, 1, 2, 9, 30);
    config.end_time = to_timestamp(2024, 1, 2, 16, 0);
    config.transaction_costs.taker_fee_bps = 1.0;
    
    // Create simulator
    ExecutionSimulator simulator(config);
    
    // Load tick data
    SyntheticTickGenerator generator(
        Symbol("AAPL"), 150.0, 0.02,
        config.start_time, config.end_time,
        NANOSECONDS_PER_MILLISECOND * 10
    );
    
    Tick tick;
    while (generator.next_tick(tick)) {
        simulator.add_tick(tick);
    }
    
    // Run simulation
    simulator.run();
    
    // Check results
    auto stats = simulator.pnl_tracker().calculate_stats();
    std::cout << "Sharpe: " << stats.sharpe_ratio << "\n";
    
    return 0;
}
```

### Python Example

```python
import pybacktest as bt

# Configure backtest
config = bt.BacktestConfig()
config.initial_capital = 1_000_000.0
config.start_time = bt.to_timestamp(2024, 1, 2, 9, 30)
config.end_time = bt.to_timestamp(2024, 1, 2, 16, 0)

# Create simulator
simulator = bt.ExecutionSimulator(config)

# Generate synthetic ticks
symbol = bt.Symbol("AAPL")
generator = bt.SyntheticTickGenerator(
    symbol, 150.0, 0.02,
    config.start_time, config.end_time,
    bt.NANOSECONDS_PER_MILLISECOND * 10
)

# Load ticks
tick = bt.Tick()
while generator.has_data():
    generator.next_tick(tick)
    simulator.add_tick(tick)

# Run simulation
simulator.run()

# Check results
pm = simulator.position_manager()
print(f"Portfolio Value: ${pm.total_portfolio_value():,.2f}")
```

---

## Strategy Development

### Built-in Strategies

#### 1. VWAP Execution
Executes a large order by targeting the Volume Weighted Average Price:

```python
vwap = bt.VWAPStrategy()
vwap.configure(symbol, bt.Side.Buy, bt.Quantity.from_int(10000),
               start_time, end_time, num_slices=20)
```

#### 2. Mean Reversion
Trades when price deviates from moving average:

```python
mean_rev = bt.MeanReversionStrategy()
mean_rev.configure(symbol, lookback_period=20, 
                   entry_z_score=2.0, exit_z_score=0.5)
```

#### 3. Market Making
Posts bids and offers around the mid-price with inventory skew:

```python
mm = bt.MarketMakingStrategy()
mm.configure(symbol, spread_bps=10.0, order_size=100,
             max_position=1000, skew_factor=0.1)
```

#### 4. Momentum
Follows price momentum over a lookback period:

```python
mom = bt.MomentumStrategy()
mom.configure(symbol, lookback_period=10, threshold_bps=50.0)
```

### Custom Strategy (Python)

```python
class MyStrategy(bt.Strategy):
    def __init__(self):
        super().__init__("MyStrategy")
        
    def on_tick(self, tick):
        # Your logic here
        if should_buy:
            self.submit_market_order(tick.symbol, bt.Side.Buy, qty)
        
    def on_fill(self, fill):
        print(f"Filled: {fill.fill_quantity} @ {fill.fill_price}")
```

### Custom Strategy (C++)

```cpp
class MyStrategy : public StrategyBase {
public:
    MyStrategy() : StrategyBase("MyStrategy") {}
    
    void on_tick(const Tick& tick) override {
        if (should_buy) {
            submit_market_order(tick.symbol, Side::Buy, qty);
        }
    }
    
    void on_fill(const Fill& fill) override {
        // Handle fill
    }
};
```

---

## Performance Considerations

### Zero-Copy Design

- Fixed-point `Price` and `Quantity` types avoid floating-point overhead
- Cache-aligned structures (64-byte alignment)
- Pre-allocated memory pools for orders

### Memory Pool Usage

```cpp
MemoryPool<Order> pool;
Order* order = pool.construct();  // ~10x faster than new
pool.destroy(order);              // Returns to pool
```

### Lock-Free Queue (SPSC)

```cpp
LockFreeEventQueue<65536> queue;  // Power of 2 capacity
queue.try_push(event);            // Non-blocking
auto event = queue.try_pop();     // Non-blocking
```

### Benchmark Results

| Operation | Time |
|-----------|------|
| Add Order to Book | ~50 ns |
| Best Bid/Ask Lookup | ~10 ns |
| Memory Pool Allocate | ~20 ns |
| Event Queue Push | ~30 ns |
| Match Order (single fill) | ~200 ns |

---

## API Reference

### Core Types

| Type | Description |
|------|-------------|
| `Price` | Fixed-point price (6 decimal places) |
| `Quantity` | Fixed-point quantity (4 decimal places) |
| `Symbol` | 16-character symbol identifier |
| `Timestamp` | Nanoseconds since epoch |

### Order Types

| Type | Description |
|------|-------------|
| `Market` | Execute immediately at best price |
| `Limit` | Execute at limit price or better |
| `IOC` | Immediate or cancel (partial fills allowed) |
| `FOK` | Fill or kill (all or nothing) |
| `GTC` | Good till cancel |

### Event Types

| Type | Description |
|------|-------------|
| `MarketDataEvent` | Tick update |
| `OrderSubmitEvent` | New order |
| `OrderCancelEvent` | Cancel request |
| `OrderFillEvent` | Execution report |
| `OrderRejectEvent` | Rejection |
| `TradeEvent` | Public trade |
| `PositionUpdateEvent` | Position change |

---

## License

MIT License - see [LICENSE](LICENSE) file.
