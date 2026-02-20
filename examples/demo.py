"""
Example Python script demonstrating the backtesting engine.
"""

import sys
sys.path.insert(0, './build')  # Add build directory to path

try:
    import pybacktest as bt
    import numpy as np
except ImportError as e:
    print(f"Import error: {e}")
    print("Make sure to build the library first:")
    print("  mkdir build && cd build && cmake .. && make")
    sys.exit(1)


def demo_basic_backtest():
    """Demonstrate basic backtesting setup."""
    print("\n" + "=" * 60)
    print("BASIC BACKTEST DEMO")
    print("=" * 60)
    
    # Configure backtest
    config = bt.BacktestConfig()
    config.initial_capital = 1_000_000.0
    config.start_time = bt.to_timestamp(2024, 1, 2, 9, 30)
    config.end_time = bt.to_timestamp(2024, 1, 2, 16, 0)
    
    # Transaction costs: 1 bps taker fee
    config.transaction_costs.taker_fee_bps = 1.0
    config.transaction_costs.maker_fee_bps = 0.0
    
    # Latency: 1 microsecond
    config.latency.order_submit_latency_ns = 1000
    config.latency.market_data_latency_ns = 500
    
    print(f"\nConfiguration:")
    print(f"  Initial Capital: ${config.initial_capital:,.0f}")
    print(f"  Taker Fee: {config.transaction_costs.taker_fee_bps} bps")
    
    # Create simulator
    simulator = bt.ExecutionSimulator(config)
    
    # Generate synthetic tick data
    symbol = bt.Symbol("AAPL")
    tick_interval = bt.NANOSECONDS_PER_MILLISECOND * 100  # 100ms
    
    generator = bt.SyntheticTickGenerator(
        symbol, 150.0, 0.02,  # $150 initial, 2% daily vol
        config.start_time, config.end_time, tick_interval
    )
    
    print(f"  Generated {generator.total_ticks():,} ticks")
    
    # Load ticks into simulator
    tick = bt.Tick()
    ticks_loaded = 0
    while generator.has_data():
        if generator.next_tick(tick):
            simulator.add_tick(tick)
            ticks_loaded += 1
    
    # Run simulation
    import time
    start = time.perf_counter()
    simulator.run()
    elapsed = (time.perf_counter() - start) * 1000
    
    print(f"\nSimulation Results:")
    print(f"  Events Processed: {simulator.total_events_processed():,}")
    print(f"  Wall Clock Time: {elapsed:.2f} ms")
    print(f"  Throughput: {ticks_loaded / elapsed * 1000:,.0f} ticks/sec")
    
    # Check position manager
    pm = simulator.position_manager()
    print(f"\nPortfolio:")
    print(f"  Cash: ${pm.cash():,.2f}")
    print(f"  Total Value: ${pm.total_portfolio_value():,.2f}")


def demo_order_submission():
    """Demonstrate order submission and execution."""
    print("\n" + "=" * 60)
    print("ORDER SUBMISSION DEMO")
    print("=" * 60)
    
    config = bt.BacktestConfig()
    config.initial_capital = 100_000.0
    config.start_time = bt.to_timestamp(2024, 1, 2, 9, 30)
    config.latency.order_submit_latency_ns = 0
    config.latency.market_data_latency_ns = 0
    
    simulator = bt.ExecutionSimulator(config)
    
    # Create market data
    symbol = bt.Symbol("AAPL")
    
    tick = bt.Tick()
    tick.timestamp = config.start_time + bt.NANOSECONDS_PER_SECOND
    tick.symbol = symbol
    tick.bid_price = bt.Price.from_double(150.0)
    tick.ask_price = bt.Price.from_double(150.05)
    tick.bid_size = bt.Quantity.from_int(1000)
    tick.ask_size = bt.Quantity.from_int(1000)
    tick.last_price = bt.Price.from_double(150.02)
    tick.last_size = bt.Quantity.from_int(100)
    
    simulator.add_tick(tick)
    simulator.process_next_event()
    
    print(f"\nMarket Data:")
    print(f"  Bid: ${tick.bid_price.to_double():.2f} x {tick.bid_size.to_int()}")
    print(f"  Ask: ${tick.ask_price.to_double():.2f} x {tick.ask_size.to_int()}")
    print(f"  Spread: ${tick.spread().to_double():.4f}")
    
    # Submit limit order
    engine = simulator.matching_engine()
    
    # Passive limit order (should rest in book)
    req = bt.OrderRequest.limit(symbol, bt.Side.Buy, 
                                bt.Quantity.from_int(100),
                                bt.Price.from_double(149.90))
    order = engine.submit_order(req)
    
    print(f"\nSubmitted passive limit order:")
    print(f"  Order ID: {order.id}")
    print(f"  Status: {order.status}")
    print(f"  Price: ${order.price.to_double():.2f}")
    print(f"  Quantity: {order.quantity.to_int()}")
    
    # Aggressive limit order (should match)
    req2 = bt.OrderRequest.limit(symbol, bt.Side.Sell, 
                                 bt.Quantity.from_int(50),
                                 bt.Price.from_double(149.90))
    order2 = engine.submit_order(req2)
    
    print(f"\nSubmitted aggressive limit order:")
    print(f"  Order ID: {order2.id}")
    print(f"  Status: {order2.status}")
    
    # Check original order was partially filled
    print(f"\nOriginal order after match:")
    print(f"  Status: {order.status}")
    print(f"  Filled: {order.filled_quantity.to_int()}")
    print(f"  Remaining: {order.remaining_quantity.to_int()}")


def demo_orderbook():
    """Demonstrate order book operations."""
    print("\n" + "=" * 60)
    print("ORDER BOOK DEMO")
    print("=" * 60)
    
    symbol = bt.Symbol("AAPL")
    engine = bt.MatchingEngine()
    
    # Add multiple price levels
    prices = [149.90, 149.95, 150.00, 150.05, 150.10]
    
    for i, price in enumerate(prices[:3]):
        req = bt.OrderRequest.limit(symbol, bt.Side.Buy,
                                    bt.Quantity.from_int(100 * (i + 1)),
                                    bt.Price.from_double(price))
        engine.submit_order(req)
    
    for i, price in enumerate(prices[2:]):
        req = bt.OrderRequest.limit(symbol, bt.Side.Sell,
                                    bt.Quantity.from_int(100 * (i + 1)),
                                    bt.Price.from_double(price))
        engine.submit_order(req)
    
    book = engine.get_order_book(symbol)
    
    print(f"\nOrder Book for {symbol.to_string()}:")
    print(f"  Best Bid: ${book.best_bid().to_double():.2f}")
    print(f"  Best Ask: ${book.best_ask().to_double():.2f}")
    print(f"  Spread: ${book.spread().to_double():.4f}")
    print(f"  Mid Price: ${book.mid_price().to_double():.4f}")
    print(f"  Micro Price: ${book.micro_price().to_double():.4f}")
    print(f"  Order Count: {book.order_count()}")


def demo_strategies():
    """Demonstrate built-in strategies."""
    print("\n" + "=" * 60)
    print("STRATEGIES DEMO")
    print("=" * 60)
    
    symbol = bt.Symbol("AAPL")
    
    # VWAP Strategy
    vwap = bt.VWAPStrategy()
    start_time = bt.to_timestamp(2024, 1, 2, 9, 30)
    end_time = bt.to_timestamp(2024, 1, 2, 10, 30)
    
    vwap.configure(symbol, bt.Side.Buy, bt.Quantity.from_int(10000),
                   start_time, end_time, 20)
    
    print(f"\nVWAP Strategy:")
    print(f"  Target: 10,000 shares over 1 hour")
    print(f"  Slices: 20")
    
    # Mean Reversion Strategy
    mean_rev = bt.MeanReversionStrategy()
    mean_rev.configure(symbol, 20, 2.0, 0.5, bt.Quantity.from_int(100))
    
    print(f"\nMean Reversion Strategy:")
    print(f"  Lookback: 20 periods")
    print(f"  Entry Z-Score: 2.0")
    print(f"  Exit Z-Score: 0.5")
    
    # Market Making Strategy
    mm = bt.MarketMakingStrategy()
    mm.configure(symbol, 10.0, bt.Quantity.from_int(100), 1000, 0.1)
    
    print(f"\nMarket Making Strategy:")
    print(f"  Spread: 10 bps")
    print(f"  Order Size: 100 shares")
    print(f"  Max Position: 1,000 shares")
    
    # Momentum Strategy
    mom = bt.MomentumStrategy()
    mom.configure(symbol, 10, 50.0, bt.Quantity.from_int(100))
    
    print(f"\nMomentum Strategy:")
    print(f"  Lookback: 10 periods")
    print(f"  Threshold: 50 bps")


def demo_performance():
    """Benchmark tick processing speed."""
    print("\n" + "=" * 60)
    print("PERFORMANCE BENCHMARK")
    print("=" * 60)
    
    config = bt.BacktestConfig()
    config.initial_capital = 1_000_000.0
    config.start_time = bt.to_timestamp(2024, 1, 2, 9, 30)
    config.end_time = bt.to_timestamp(2024, 1, 2, 16, 0)  # Full trading day
    
    simulator = bt.ExecutionSimulator(config)
    
    # Generate one day of ticks at 10ms intervals
    # 6.5 hours = 23,400 seconds = 2,340,000 ticks at 10ms
    symbol = bt.Symbol("AAPL")
    tick_interval = bt.NANOSECONDS_PER_MILLISECOND * 10  # 10ms
    
    generator = bt.SyntheticTickGenerator(
        symbol, 150.0, 0.02,
        config.start_time, config.end_time, tick_interval
    )
    
    expected_ticks = generator.total_ticks()
    print(f"\nGenerating {expected_ticks:,} ticks (one trading day at 10ms intervals)")
    
    # Load all ticks
    import time
    load_start = time.perf_counter()
    
    tick = bt.Tick()
    tick_count = 0
    while generator.has_data():
        if generator.next_tick(tick):
            simulator.add_tick(tick)
            tick_count += 1
    
    load_time = (time.perf_counter() - load_start) * 1000
    
    # Process all events
    process_start = time.perf_counter()
    simulator.run()
    process_time = (time.perf_counter() - process_start) * 1000
    
    total_time = load_time + process_time
    
    print(f"\nResults:")
    print(f"  Ticks Loaded: {tick_count:,}")
    print(f"  Load Time: {load_time:.2f} ms")
    print(f"  Process Time: {process_time:.2f} ms")
    print(f"  Total Time: {total_time:.2f} ms")
    print(f"  Throughput: {tick_count / total_time * 1000:,.0f} ticks/sec")
    
    # Calculate speedup vs real-time
    sim_hours = 6.5  # Trading hours
    real_seconds = total_time / 1000
    speedup = sim_hours * 3600 / real_seconds
    
    print(f"\n  Simulated: {sim_hours} hours")
    print(f"  Real Time: {real_seconds:.3f} seconds")
    print(f"  Speedup: {speedup:,.0f}x real-time")
    
    if total_time < 1000:
        print(f"\n  ✓ Target achieved: 1 day of ticks in < 1 second!")
    else:
        print(f"\n  Target: 1 day in < 1 second (current: {total_time:.0f} ms)")


def main():
    """Run all demos."""
    print("=" * 60)
    print("LOW-LATENCY BACKTESTING ENGINE - PYTHON DEMO")
    print("=" * 60)
    
    try:
        demo_basic_backtest()
        demo_order_submission()
        demo_orderbook()
        demo_strategies()
        demo_performance()
        
        print("\n" + "=" * 60)
        print("ALL DEMOS COMPLETED SUCCESSFULLY")
        print("=" * 60)
        
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
