#include <benchmark/benchmark.h>
#include "engine/matching_engine.h"
#include "concurrency/spsc_queue.h"
#include "memory/object_pool.h"
#include <bit>

using namespace lfe;

static void BM_OrderInsert(benchmark::State& state) {
    size_t q_cap = std::bit_ceil(static_cast<size_t>(state.range(0) * 2));
    SPSCQueue<OutboundMessage> egress(q_cap);
    MatchingEngine engine(state.range(0), egress);
    
    std::vector<OrderRequest> requests;
    for (int i = 0; i < state.range(0); ++i) {
        requests.push_back({OrderRequest::Action::New, (uint64_t)i, (uint64_t)(100 + i % 10), 10, Side::Buy, OrderType::Limit, 1000});
    }

    for (auto _ : state) {
        for (const auto& req : requests) {
            engine.process(req);
        }
        state.PauseTiming();
        OutboundMessage msg;
        while(egress.dequeue(msg));
        state.ResumeTiming();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

static void BM_ObjectPoolAllocRelease(benchmark::State& state) {
    ObjectPool<Order> pool(state.range(0));
    std::vector<Order*> allocs;
    allocs.reserve(state.range(0));

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i) {
            allocs.push_back(pool.allocate(i, 100, 10, Side::Buy, OrderType::Limit, 1000));
        }
        for (auto* ptr : allocs) {
            pool.release(ptr);
        }
        allocs.clear();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0) * 2);
}

BENCHMARK(BM_OrderInsert)->Arg(1000)->Arg(100000)->Arg(1000000);
BENCHMARK(BM_ObjectPoolAllocRelease)->Arg(1000)->Arg(100000)->Arg(1000000);

BENCHMARK_MAIN();
