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

The system is built around a single-threaded core matching engine which operates entirely lock-free on the hot path, achieving ultra-low latencies.

### 1. Threading Model & Communication
- **Matching Engine Thread**: A dedicated thread handles order matching and book updates.
- **Lock-Free Queues**: 
  - **MPSC Queue** (Multi-Producer, Single-Consumer): Ingests `OrderRequest` messages from multiple gateway threads into the matching engine.
  - **SPSC Queue** (Single-Producer, Single-Consumer): Publishes `OutboundMessage` (trades, market data) from the engine to downstream consumers without blocking.
- **Cache Alignment**: Critical data structures are padded and aligned (`alignas(64)`) to prevent false sharing across CPU cores.

### 2. Core Order Book
The matching engine uses a strict **Price-Time Priority** matching algorithm:
- **Price Levels**: Managed efficiently for rapid Best Bid/Offer (BBO) lookups.
- **Intrusive Doubly-Linked Lists**: Orders at a specific price level are maintained in a linked list. This enables **O(1) time complexity** for order removals and cancellations.
- **Order Registry**: Direct memory access linking an `OrderID` to its `Order` object, allowing instant O(1) lookups.

### 3. Memory Management
- **Zero-Allocation**: The system forbids dynamic memory allocation (`new`/`delete`) during active trading.
- **Object Pool**: A custom pre-allocated lock-free pool manages memory for `Order` objects. When an order is filled or cancelled, its memory is instantly returned to the pool for reuse, preventing OS-level heap contention.

### 4. Metrics & Logging
- **Asynchronous Logger**: A background thread consumes log messages via an SPSC queue, ensuring the matching engine is never blocked by disk I/O.
- **Latency Tracking**: High-resolution CPU cycle counters capture nanosecond-level percentiles (P50, P90, P99).

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
