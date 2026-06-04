#include "concurrency/mpsc_queue.h"
#include "concurrency/spsc_queue.h"
#include "engine/matching_engine.h"
#include "io/logger.h"
#include "io/market_data.h"
#include "io/persistence.h"
#include "metrics/metrics.h"
#include <thread>
#include <vector>
#include <iostream>

using namespace lfe;

void producer_thread(MPSCQueue<OrderRequest>& ingress, size_t num_orders, OrderId start_id) {
    for (size_t i = 0; i < num_orders; ++i) {
        OrderRequest req;
        req.action = OrderRequest::Action::New;
        req.id = start_id + i;
        req.price = 100 + (i % 10); // prices 100 to 109
        req.quantity = 100;
        req.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        req.type = OrderType::Limit;
        req.timestamp = now_ns();
        
        while (!ingress.enqueue(req)) {
            // spin wait
        }
    }
}

int main() {
    std::cout << "Starting Lock-Free Exchange...\n";

    constexpr size_t INGRESS_CAPACITY = 65536;
    constexpr size_t EGRESS_CAPACITY = 65536;
    constexpr size_t MAX_ORDERS = 1000000;

    MPSCQueue<OrderRequest> ingress_queue(INGRESS_CAPACITY);
    SPSCQueue<OutboundMessage> egress_queue(EGRESS_CAPACITY);

    AsyncLogger logger("exchange.log");
    MarketDataPublisher market_data;
    Metrics metrics;

    std::atomic<bool> running{true};

    // Matching Engine Thread
    std::thread engine_thread([&]() {
        MatchingEngine engine(MAX_ORDERS, egress_queue);
        OrderRequest req;
        while (running.load(std::memory_order_acquire)) {
            if (ingress_queue.dequeue(req)) {
                uint64_t start = now_ns();
                engine.process(req);
                uint64_t end = now_ns();
                metrics.record_latency(start, end);
            }
        }
    });

    // Market Data & Egress Thread
    std::thread egress_thread([&]() {
        OutboundMessage msg;
        while (running.load(std::memory_order_acquire)) {
            if (egress_queue.dequeue(msg)) {
                market_data.process_message(msg);
            }
        }
    });

    // Start producers
    std::cout << "Starting traffic...\n";
    std::thread p1(producer_thread, std::ref(ingress_queue), 10000, 1);
    std::thread p2(producer_thread, std::ref(ingress_queue), 10000, 10001);

    p1.join();
    p2.join();

    // Give some time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    running.store(false, std::memory_order_release);
    engine_thread.join();
    egress_thread.join();

    std::cout << "Traffic completed.\n";
    market_data.print_snapshot();
    metrics.print_report();

    return 0;
}
