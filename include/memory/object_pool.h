#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <cassert>

namespace lfe {

// A high-performance, single-threaded object pool.
// Pre-allocates a fixed number of elements to avoid dynamic allocation during runtime.
template <typename T>
class alignas(64) ObjectPool {
public:
    explicit ObjectPool(size_t capacity) : capacity_(capacity), free_head_(0) {
        // Allocate raw memory for the pool
        data_ = static_cast<Node*>(::operator new[](capacity_ * sizeof(Node), std::align_val_t{64}));
        
        // Initialize the free list
        for (size_t i = 0; i < capacity_ - 1; ++i) {
            data_[i].next_free = i + 1;
        }
        data_[capacity_ - 1].next_free = INVALID_INDEX;
    }

    ~ObjectPool() {
        ::operator delete[](data_, std::align_val_t{64});
    }

    // Disallow copy and move for simplicity
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (free_head_ == INVALID_INDEX) {
            return nullptr; // Pool exhausted
        }

        size_t index = free_head_;
        free_head_ = data_[index].next_free;

        T* ptr = reinterpret_cast<T*>(&data_[index].storage);
        new (ptr) T(std::forward<Args>(args)...);

        return ptr;
    }

    void release(T* ptr) {
        if (!ptr) return;

        ptr->~T();

        // Calculate index
        Node* node = reinterpret_cast<Node*>(ptr);
        size_t index = node - data_;
        
        assert(index < capacity_ && "Pointer does not belong to this pool");

        data_[index].next_free = free_head_;
        free_head_ = index;
    }

private:
    static constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);

    union Node {
        std::aligned_storage_t<sizeof(T), alignof(T)> storage;
        size_t next_free;
    };

    Node* data_{nullptr};
    size_t capacity_{0};
    size_t free_head_{0};
};

} // namespace lfe
