#pragma once

#include <atomic>
#include <vector>
#include <cassert>
#include <cstddef>
#include <new>

namespace lfe {

// A bounded Multi-Producer Single-Consumer lock-free ring buffer.
// Optimized for throughput and low latency.
template <typename T>
class alignas(64) MPSCQueue {
public:
    explicit MPSCQueue(size_t capacity)
        : capacity_(capacity), mask_(capacity - 1) {
        // Capacity must be a power of 2 for fast modulo using bitwise AND
        assert((capacity > 0) && ((capacity & (capacity - 1)) == 0) && "Capacity must be power of 2");
        buffer_ = static_cast<Slot*>(::operator new[](capacity_ * sizeof(Slot), std::align_val_t{64}));
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~MPSCQueue() {
        ::operator delete[](buffer_, std::align_val_t{64});
    }

    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    // Called by multiple producer threads
    template <typename... Args>
    bool enqueue(Args&&... args) {
        Slot* slot = nullptr;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        
        while (true) {
            slot = &buffer_[pos & mask_];
            size_t seq = slot->sequence.load(std::memory_order_acquire);
            intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (dif == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                return false; // Queue is full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        new (&slot->data) T(std::forward<Args>(args)...);
        slot->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    // Called by the single consumer thread
    bool dequeue(T& data) {
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        Slot* slot = &buffer_[pos & mask_];
        size_t seq = slot->sequence.load(std::memory_order_acquire);
        intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

        if (dif == 0) {
            data = std::move(*reinterpret_cast<T*>(&slot->data));
            reinterpret_cast<T*>(&slot->data)->~T();
            slot->sequence.store(pos + capacity_, std::memory_order_release);
            dequeue_pos_.store(pos + 1, std::memory_order_relaxed);
            return true;
        }

        return false; // Queue is empty
    }

private:
    struct alignas(64) Slot {
        std::atomic<size_t> sequence;
        typename std::aligned_storage<sizeof(T), alignof(T)>::type data;
    };

    const size_t capacity_;
    const size_t mask_;
    Slot* buffer_;

    // Align indices to different cache lines to prevent false sharing
    alignas(64) std::atomic<size_t> enqueue_pos_;
    alignas(64) std::atomic<size_t> dequeue_pos_;
};

} // namespace lfe
