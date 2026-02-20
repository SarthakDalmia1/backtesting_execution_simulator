#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <numeric>

#include "core/types.hpp"
#include "core/utils.hpp"
#include "core/memory_pool.hpp"
#include "orderbook/order.hpp"
#include "orderbook/orderbook.hpp"
#include "orderbook/matching_engine.hpp"
#include "events/event_queue.hpp"
#include "execution/execution_simulator.hpp"
#include "data/market_data_feed.hpp"
#include "strategy/strategies.hpp"

using namespace backtest;

// ============================================================================
// Benchmark Utilities
// ============================================================================
class Benchmark {
public:
    Benchmark(const std::string& name) : name_(name) {}
    
    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    void stop() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
        durations_.push_back(duration.count());
    }
    
    void report() const {
        if (durations_.empty()) return;
        
        double sum = std::accumulate(durations_.begin(), durations_.end(), 0.0);
        double mean = sum / durations_.size();
        
        double sq_sum = 0.0;
        for (auto d : durations_) {
            sq_sum += (d - mean) * (d - mean);
        }
        double stddev = std::sqrt(sq_sum / durations_.size());
        
        auto min_it = std::min_element(durations_.begin(), durations_.end());
        auto max_it = std::max_element(durations_.begin(), durations_.end());
        
        std::cout << "\n" << name_ << ":\n";
        std::cout << "  Iterations: " << durations_.size() << "\n";
        std::cout << "  Mean:       " << std::fixed << std::setprecision(2) 
                  << mean / 1000.0 << " μs\n";
        std::cout << "  Std Dev:    " << stddev / 1000.0 << " μs\n";
        std::cout << "  Min:        " << *min_it / 1000.0 << " μs\n";
        std::cout << "  Max:        " << *max_it / 1000.0 << " μs\n";
    }
    
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
    std::vector<int64_t> durations_;
};

// ============================================================================
// Benchmark: Order Book Operations
// ============================================================================
void benchmark_orderbook_operations() {
    std::cout << "\n========================================\n";
    std::cout << "ORDER BOOK BENCHMARKS\n";
    std::cout << "========================================\n";
    
    OrderBook book(Symbol("AAPL"));
    MemoryPool<Order> pool;
    
    // Benchmark add order
    {
        Benchmark bm("Add Order");
        for (int i = 0; i < 100000; ++i) {
            Order* order = pool.construct();
            order->id = i + 1;
            order->symbol = Symbol("AAPL");
            order->side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            order->price = Price::from_double(100.0 + (i % 100) * 0.01);
            order->quantity = Quantity::from_int(100);
            order->remaining_quantity = order->quantity;
            
            bm.start();
            book.add_order(order);
            bm.stop();
        }
        bm.report();
    }
    
    // Benchmark best bid/ask
    {
        Benchmark bm("Get Best Bid/Ask");
        for (int i = 0; i < 100000; ++i) {
            bm.start();
            Price bid = book.best_bid();
            Price ask = book.best_ask();
            bm.stop();
            (void)bid; (void)ask;
        }
        bm.report();
    }
    
    // Benchmark mid price
    {
        Benchmark bm("Get Mid Price");
        for (int i = 0; i < 100000; ++i) {
            bm.start();
            Price mid = book.mid_price();
            bm.stop();
            (void)mid;
        }
        bm.report();
    }
}

// ============================================================================
// Benchmark: Matching Engine
// ============================================================================
void benchmark_matching_engine() {
    std::cout << "\n========================================\n";
    std::cout << "MATCHING ENGINE BENCHMARKS\n";
    std::cout << "========================================\n";
    
    MatchingEngine engine;
    Symbol symbol("AAPL");
    
    // Pre-populate with passive orders
    for (int i = 0; i < 10000; ++i) {
        OrderRequest req;
        req.symbol = symbol;
        req.side = Side::Buy;
        req.type = OrderType::Limit;
        req.quantity = Quantity::from_int(100);
        req.price = Price::from_double(99.0 + i * 0.001);
        engine.submit_order(req);
        
        req.side = Side::Sell;
        req.price = Price::from_double(101.0 + i * 0.001);
        engine.submit_order(req);
    }
    
    // Benchmark limit order submission
    {
        Benchmark bm("Submit Limit Order");
        for (int i = 0; i < 10000; ++i) {
            OrderRequest req;
            req.symbol = symbol;
            req.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            req.type = OrderType::Limit;
            req.quantity = Quantity::from_int(100);
            req.price = Price::from_double(100.0);
            
            bm.start();
            engine.submit_order(req);
            bm.stop();
        }
        bm.report();
    }
    
    // Benchmark market order (crossing spread)
    MatchingEngine engine2;
    for (int i = 0; i < 1000; ++i) {
        OrderRequest req;
        req.symbol = symbol;
        req.side = Side::Buy;
        req.type = OrderType::Limit;
        req.quantity = Quantity::from_int(100);
        req.price = Price::from_double(99.0);
        engine2.submit_order(req);
        
        req.side = Side::Sell;
        req.price = Price::from_double(101.0);
        engine2.submit_order(req);
    }
    
    {
        Benchmark bm("Submit Market Order (matching)");
        for (int i = 0; i < 1000; ++i) {
            OrderRequest req;
            req.symbol = symbol;
            req.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            req.type = OrderType::Market;
            req.quantity = Quantity::from_int(50);
            
            bm.start();
            engine2.submit_order(req);
            bm.stop();
        }
        bm.report();
    }
}

// ============================================================================
// Benchmark: Event Queue
// ============================================================================
void benchmark_event_queue() {
    std::cout << "\n========================================\n";
    std::cout << "EVENT QUEUE BENCHMARKS\n";
    std::cout << "========================================\n";
    
    EventQueue queue;
    
    // Benchmark push
    {
        Benchmark bm("Push Event");
        for (int i = 0; i < 100000; ++i) {
            Tick tick;
            tick.timestamp = i;
            tick.symbol = Symbol("AAPL");
            tick.bid_price = Price::from_double(100.0);
            tick.ask_price = Price::from_double(100.01);
            
            MarketDataEvent event(tick);
            
            bm.start();
            queue.push(event);
            bm.stop();
        }
        bm.report();
    }
    
    // Benchmark pop
    {
        Benchmark bm("Pop Event");
        for (int i = 0; i < 100000; ++i) {
            bm.start();
            auto event = queue.pop();
            bm.stop();
            (void)event;
        }
        bm.report();
    }
}

// ============================================================================
// Benchmark: Full Simulation (Tick Processing)
// ============================================================================
void benchmark_tick_processing() {
    std::cout << "\n========================================\n";
    std::cout << "TICK PROCESSING BENCHMARKS\n";
    std::cout << "========================================\n";
    
    // Generate synthetic ticks
    Symbol symbol("AAPL");
    Timestamp start_time = to_timestamp(2024, 1, 1, 9, 30, 0);
    Timestamp end_time = to_timestamp(2024, 1, 1, 16, 0, 0);
    
    // Generate one day of ticks at 10ms intervals
    // 6.5 hours = 23,400 seconds = 2,340,000 ticks at 10ms
    int64_t tick_interval = NANOSECONDS_PER_MILLISECOND * 10;
    
    SyntheticTickGenerator generator(symbol, 100.0, 0.02, 
                                     start_time, end_time, tick_interval);
    
    std::vector<Tick> ticks;
    ticks.reserve(generator.total_ticks());
    
    Tick tick;
    while (generator.next_tick(tick)) {
        ticks.push_back(tick);
    }
    
    std::cout << "Generated " << ticks.size() << " ticks\n";
    
    // Benchmark processing speed
    {
        BacktestConfig config;
        config.start_time = start_time;
        config.end_time = end_time;
        config.initial_capital = 1000000.0;
        
        ExecutionSimulator simulator(config);
        
        auto bench_start = std::chrono::high_resolution_clock::now();
        
        for (const auto& t : ticks) {
            simulator.add_tick(t);
        }
        
        simulator.run();
        
        auto bench_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            bench_end - bench_start);
        
        std::cout << "\nFull Simulation Benchmark:\n";
        std::cout << "  Ticks Processed: " << ticks.size() << "\n";
        std::cout << "  Events Processed: " << simulator.total_events_processed() << "\n";
        std::cout << "  Wall Clock Time: " << duration.count() << " ms\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0)
                  << (ticks.size() * 1000.0 / duration.count()) << " ticks/sec\n";
        
        // Calculate simulated time
        double sim_hours = (end_time - start_time) / 
                          static_cast<double>(NANOSECONDS_PER_SECOND) / 3600.0;
        std::cout << "  Simulated Time: " << std::setprecision(2) << sim_hours << " hours\n";
        std::cout << "  Speedup: " << std::setprecision(0) 
                  << (sim_hours * 3600.0 * 1000.0 / duration.count()) << "x realtime\n";
    }
}

// ============================================================================
// Benchmark: Memory Pool
// ============================================================================
void benchmark_memory_pool() {
    std::cout << "\n========================================\n";
    std::cout << "MEMORY POOL BENCHMARKS\n";
    std::cout << "========================================\n";
    
    // Benchmark memory pool allocation
    {
        MemoryPool<Order> pool;
        Benchmark bm("Memory Pool Allocate");
        
        std::vector<Order*> orders;
        orders.reserve(100000);
        
        for (int i = 0; i < 100000; ++i) {
            bm.start();
            Order* o = pool.allocate();
            bm.stop();
            orders.push_back(o);
        }
        bm.report();
        
        // Benchmark deallocation
        Benchmark bm2("Memory Pool Deallocate");
        for (Order* o : orders) {
            bm2.start();
            pool.deallocate(o);
            bm2.stop();
        }
        bm2.report();
    }
    
    // Compare with standard new/delete
    {
        Benchmark bm("Standard new");
        std::vector<Order*> orders;
        orders.reserve(100000);
        
        for (int i = 0; i < 100000; ++i) {
            bm.start();
            Order* o = new Order();
            bm.stop();
            orders.push_back(o);
        }
        bm.report();
        
        Benchmark bm2("Standard delete");
        for (Order* o : orders) {
            bm2.start();
            delete o;
            bm2.stop();
        }
        bm2.report();
    }
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "BACKTESTING ENGINE BENCHMARKS\n";
    std::cout << "========================================\n";
    
    try {
        benchmark_memory_pool();
        benchmark_orderbook_operations();
        benchmark_matching_engine();
        benchmark_event_queue();
        benchmark_tick_processing();
        
        std::cout << "\n========================================\n";
        std::cout << "ALL BENCHMARKS COMPLETED\n";
        std::cout << "========================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
