#pragma once

#include <atomic>
#include <cstddef>
#include <cassert>
#include <new>
#include <utility>

namespace lfe {

// A bounded Single-Producer Single-Consumer lock-free ring buffer.
// Faster than MPSC since it doesn't require CAS for the producer.
template <typename T>
class alignas(64) SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity)
        : capacity_(capacity), mask_(capacity - 1) {
        assert((capacity > 0) && ((capacity & (capacity - 1)) == 0) && "Capacity must be power of 2");
        buffer_ = static_cast<T*>(::operator new[](capacity_ * sizeof(T), std::align_val_t{64}));
        write_idx_.store(0, std::memory_order_relaxed);
        read_idx_.store(0, std::memory_order_relaxed);
    }

    ~SPSCQueue() {
        // Destroy any remaining elements
        size_t read = read_idx_.load(std::memory_order_relaxed);
        size_t write = write_idx_.load(std::memory_order_relaxed);
        while (read != write) {
            buffer_[read & mask_].~T();
            ++read;
        }
        ::operator delete[](buffer_, std::align_val_t{64});
    }

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Called only by the producer thread
    template <typename... Args>
    bool enqueue(Args&&... args) {
        const size_t write = write_idx_.load(std::memory_order_relaxed);
        // Load read_idx with acquire to ensure we see the latest value from the consumer
        // We only need to load it if the queue might be full, but caching it helps.
        // For strict SPSC, we can cache the read index to avoid constant atomic loads.
        const size_t next_write = write + 1;

        if (next_write - cached_read_idx_ > capacity_) {
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if (next_write - cached_read_idx_ > capacity_) {
                return false; // Queue is full
            }
        }

        new (&buffer_[write & mask_]) T(std::forward<Args>(args)...);
        write_idx_.store(next_write, std::memory_order_release);
        return true;
    }

    // Called only by the consumer thread
    bool dequeue(T& data) {
        const size_t read = read_idx_.load(std::memory_order_relaxed);

        if (read == cached_write_idx_) {
            cached_write_idx_ = write_idx_.load(std::memory_order_acquire);
            if (read == cached_write_idx_) {
                return false; // Queue is empty
            }
        }

        data = std::move(buffer_[read & mask_]);
        buffer_[read & mask_].~T();
        read_idx_.store(read + 1, std::memory_order_release);
        return true;
    }

private:
    const size_t capacity_;
    const size_t mask_;
    T* buffer_;

    // Align elements to prevent false sharing
    alignas(64) std::atomic<size_t> write_idx_;
    alignas(64) size_t cached_read_idx_{0}; // Only written and read by producer

    alignas(64) std::atomic<size_t> read_idx_;
    alignas(64) size_t cached_write_idx_{0}; // Only written and read by consumer
};

} // namespace lfe
