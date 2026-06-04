#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include <chrono>

namespace lfe {

// A simple utility to measure high-resolution time
inline uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

class Metrics {
public:
    Metrics() {
        latencies_.reserve(1000000); // Pre-reserve to avoid allocation during hot path
    }

    void record_latency(uint64_t start_ns, uint64_t end_ns) {
        if (end_ns > start_ns) {
            latencies_.push_back(end_ns - start_ns);
        }
    }

    void print_report() {
        if (latencies_.empty()) {
            std::cout << "No metrics collected.\n";
            return;
        }

        std::sort(latencies_.begin(), latencies_.end());

        size_t count = latencies_.size();
        uint64_t p50 = latencies_[count * 0.5];
        uint64_t p90 = latencies_[count * 0.9];
        uint64_t p99 = latencies_[count * 0.99];
        uint64_t p999 = latencies_[count * 0.999];
        
        uint64_t sum = 0;
        for (auto l : latencies_) sum += l;
        uint64_t avg = sum / count;

        std::cout << "--- Performance Metrics ---\n";
        std::cout << "Orders Processed: " << count << "\n";
        std::cout << "Average Latency:  " << avg << " ns\n";
        std::cout << "P50 Latency:      " << p50 << " ns\n";
        std::cout << "P90 Latency:      " << p90 << " ns\n";
        std::cout << "P99 Latency:      " << p99 << " ns\n";
        std::cout << "P99.9 Latency:    " << p999 << " ns\n";
        std::cout << "---------------------------\n";
    }

private:
    std::vector<uint64_t> latencies_;
};

} // namespace lfe
