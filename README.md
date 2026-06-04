# Lock-Free Limit Order Book

A production-grade, low-latency C++20 matching engine designed for high-frequency trading (HFT) principles.

## Features

- **Strict Price-Time Priority**: Matches aggressive orders against resting liquidity.
- **Lock-Free Concurrency**: Employs MPSC and SPSC ring buffers for thread communication. No `std::mutex` is used in the hot path.
- **Zero-Allocation**: Custom cache-aligned Object Pools prevent `new`/`delete` latency spikes during runtime.
- **O(1) Operations**: Uses an Order Registry and Intrusive Doubly-Linked Lists for O(1) order cancellation and modification.
- **Cache Optimization**: Data structures are padded and aligned (`alignas(64)`) to prevent false sharing and cache ping-ponging.
- **Metrics**: High-resolution latency tracking (P50, P90, P99).
- **Educational Guide**: Includes a comprehensive interview guide detailing HFT architecture.

## Architecture

```text
Gateways (Producers) ---> [MPSC Queue] ---> Matching Engine (Consumer) ---> [SPSC Queue] ---> Market Data / Logger
```

## Building

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Running

```bash
./lfe_engine
```
