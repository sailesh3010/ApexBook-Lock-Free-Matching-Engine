# Low-Latency C++ Engineer Interview Guide

This guide covers the core concepts demonstrated in the Lock-Free Order Book project, specifically tailored for quantitative trading and High-Frequency Trading (HFT) interviews.

## 1. Why Price-Time Priority Exists
Exchanges use Price-Time priority to ensure fairness and encourage liquidity provision.
- **Price Priority**: The most aggressive orders (highest bids, lowest asks) are matched first. This ensures the best execution price for market participants.
- **Time Priority**: Among orders at the same price, the first to arrive is the first to be executed. This incentivizes market makers to quote early and provide liquidity, as they are rewarded with the highest queue position.

## 2. How Exchanges Work
A limit order book (LOB) exchange operates as a central limit order book.
- **Ingestion**: Gateways receive orders via protocols like FIX or binary ITCH/OUCH.
- **Matching Engine**: Maintains the LOB. It processes orders sequentially to ensure deterministic state transitions. If an incoming order crosses the spread, it generates a trade. Otherwise, it rests in the book.
- **Market Data**: The exchange publishes incremental updates (BBO changes, trades, order additions/cancellations) so participants can reconstruct the LOB locally.

## 3. Why Lock-Free Systems Matter
In HFT, traditional locks (`std::mutex`) cause non-deterministic latency spikes due to OS context switching, thread suspension, and cache invalidation.
- **Lock-Free**: Guarantees that at least one thread in the system makes progress. By avoiding syscalls, the matching engine remains entirely in userspace, achieving sub-microsecond latencies.
- **Wait-Free**: A stronger guarantee where *every* thread makes progress.

## 4. How Atomics Work
`std::atomic` provides operations that are indivisible from the perspective of other threads. At the hardware level (x86), this is often implemented using cache coherence protocols (MESI) and locked instructions (e.g., `LOCK CMPXCHG`). Atomics prevent data races without yielding to the OS.

## 5. Memory Ordering
Memory ordering dictates how memory operations surrounding an atomic operation are visible to other threads.
- **Relaxed**: No synchronization or ordering guarantees, only atomicity. Used for counters.
- **Acquire/Release**: A release operation synchronizes with an acquire operation. All memory writes before the release become visible to the thread performing the acquire. Crucial for lock-free queues (Producer uses Release, Consumer uses Acquire).
- **Sequential Consistency**: The default. Guarantees a single total order of all atomic operations across all threads. Expensive due to full memory barriers.

## 6. False Sharing
Modern CPUs fetch memory in chunks called "Cache Lines" (typically 64 bytes). If two threads modify independent variables that happen to reside on the same cache line, the CPU must constantly invalidate and transfer the cache line between cores, destroying performance.
- **Solution**: Use `alignas(64)` to force independent atomic variables (like the read and write indices of a queue) onto separate cache lines.

## 7. Cache Locality
Accessing RAM is slow (~100ns). Accessing L1 cache is fast (~1ns). Data structures must be designed to fit in cache and be accessed sequentially.
- **Object Pools**: Pre-allocating objects in contiguous arrays improves spatial locality.
- **Flat Arrays**: Using `std::vector` or flat arrays instead of linked lists/maps ensures hardware prefetchers can predict memory access patterns.

## 8. Ring Buffers (MPSC/SPSC)
A ring buffer is a fixed-size circular array used for inter-thread communication.
- **SPSC (Single-Producer Single-Consumer)**: The fastest queue. One atomic index for reading, one for writing. No Compare-And-Swap (CAS) required.
- **MPSC (Multi-Producer Single-Consumer)**: Used for gateways sending orders to the engine. Requires a CAS loop for producers to claim a slot, but the consumer is lock-free. Capacity must be a power of 2 for fast modulo operations using bitwise AND (`index & mask`).

## 9. Custom Allocators
Dynamic memory allocation (`new`/`delete`) relies on a global heap manager, which uses locks and can trigger expensive OS page faults.
- **Object Pools**: HFT systems pre-allocate all memory at startup. When an `Order` is needed, it is taken from the pool in O(1) time. When cancelled, it is returned to the free list.

## 10. Matching Engine Architecture
A modern HFT matching engine architecture:
1. **Thread Pinning**: The engine runs on an isolated CPU core.
2. **Zero Allocation**: No dynamic memory is allocated during the trading session.
3. **O(1) Data Structures**: 
   - **Order Registry**: A flat array indexed by `OrderId` provides O(1) order lookup for cancellations.
   - **Intrusive Lists**: Orders themselves contain `prev`/`next` pointers, allowing O(1) removal from a price level.
4. **Deterministic Sequential Processing**: A single thread processes all orders to guarantee strict price-time priority without internal locking.
