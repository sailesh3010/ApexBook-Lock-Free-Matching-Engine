#pragma once

#include "core/order.h"
#include <cstdint>

namespace lfe {

// Represents a single price level containing a queue of orders.
// Uses an intrusive doubly-linked list for O(1) insertion and deletion.
class PriceLevel {
public:
    PriceLevel() : head_(nullptr), tail_(nullptr), volume_(0) {}

    // O(1) appending to the tail (maintains FIFO)
    void append(Order* order) {
        if (!head_) {
            head_ = tail_ = order;
            order->prev = nullptr;
            order->next = nullptr;
        } else {
            tail_->next = order;
            order->prev = tail_;
            order->next = nullptr;
            tail_ = order;
        }
        volume_ += order->remaining_quantity;
    }

    // O(1) removal given the order pointer
    void remove(Order* order) {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head_ = order->next;
        }

        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail_ = order->prev;
        }

        order->prev = nullptr;
        order->next = nullptr;
        volume_ -= order->remaining_quantity;
    }

    // Returns the first order (highest time priority)
    Order* front() const {
        return head_;
    }

    // Removes and returns the first order
    Order* pop_front() {
        if (!head_) return nullptr;
        Order* order = head_;
        remove(order);
        return order;
    }

    bool empty() const {
        return head_ == nullptr;
    }

    Quantity volume() const {
        return volume_;
    }

    // Call this if the quantity of an order within this level changes
    void reduce_volume(Quantity amount) {
        volume_ -= amount;
    }

private:
    Order* head_;
    Order* tail_;
    Quantity volume_; // Total volume available at this price level
};

} // namespace lfe
