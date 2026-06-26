# Benchmark Results

Real numbers from running `./benchmarks` on my machine, not aspirational targets.
The harness reports mean / std-dev / min / max over a fixed iteration count after a
warmup. Absolute latencies are machine dependent; rerun it locally and the relative
picture (pool vs `new`, book vs matching-engine cost, end-to-end throughput) will hold.

If a number here disagrees with what `./benchmarks` prints on the current code, the
printed output wins — please open an issue.

## How to reproduce

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./benchmarks
```

## Test machine

| | |
|---|---|
| CPU | AMD Ryzen AI MAX+ PRO 395 (16 cores / 32 threads) |
| Compiler | GCC 13.3.0 |
| Flags | `-O3 -march=native -DNDEBUG -ffast-math -funroll-loops` |
| OS | Ubuntu 24.04 |

## Memory pool vs standard allocator

| Operation | Mean | Min |
|-----------|------|-----|
| Pool allocate | 0.02 µs | 0.01 µs |
| Pool deallocate | 0.08 µs | 0.01 µs |
| `new` | 0.10 µs | 0.03 µs |
| `delete` | 0.05 µs | 0.02 µs |

The pre-allocated pool is ~5× faster than `new` on the hot allocate path and, more
importantly for a tight event loop, far more predictable — note the `new` max of
231 µs vs the pool's 24 µs (those tails are page faults / arena growth in the
general allocator).

## Order book

| Operation | Mean | Min |
|-----------|------|-----|
| Add order | 0.04 µs | 0.02 µs |
| Best bid/ask lookup | 0.02 µs | 0.01 µs |
| Mid price | 0.02 µs | 0.01 µs |

Best bid/ask is ~20 ns because the book keeps sorted price levels with O(1) access
to the top of book; adding an order is ~40 ns including the level bookkeeping.

## Matching engine

| Operation | Mean | Min |
|-----------|------|-----|
| Submit limit order | 0.21 µs | 0.07 µs |
| Submit market order (matching) | 0.17 µs | 0.10 µs |

## Event queue

| Operation | Mean | Min |
|-----------|------|-----|
| Push | 0.22 µs | 0.03 µs |
| Pop | 0.22 µs | 0.02 µs |

## End-to-end: full-day replay

| Metric | Value |
|--------|-------|
| Ticks processed | 2,340,000 |
| Wall-clock time | **1,519 ms** |
| Throughput | **1.54M ticks/sec** |
| Simulated time | 6.5 hours |
| Speedup vs realtime | ~15,400× |

### Honest reading of these numbers

The original README headline was "replay 1 day of ticks in < 1 second" at "2M+
ticks/sec". On this run a full 6.5-hour session of 2.34M ticks replays in **1.52 s
at 1.54M ticks/sec** — close to, but not quite, those targets. I'd rather state the
measured number than keep a round figure I can't reproduce on demand. Hitting a hard
sub-second / 2M-ticks/sec target is a concrete next optimisation (candidates:
shrinking the per-event allocation, batching the position/markout updates, and the
SPSC lock-free queue path for the multi-threaded feed).

The throughput also depends on what runs per tick — these figures are for the core
event/matching loop. Layering on heavier per-fill analytics (e.g. the markout
analyzer) will move them, which is the point of measuring rather than guessing.
