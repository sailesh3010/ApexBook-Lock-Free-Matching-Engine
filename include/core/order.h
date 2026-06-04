#pragma once

#include "core/types.h"
#include <cstdint>

namespace lfe {

// Represents an order inside the order book.
// Aligned to 64 bytes (typical cache line size) to prevent false sharing and ensure 
// optimal access patterns.
struct alignas(64) Order {
    OrderId id;
    Price price;
    Quantity quantity;
    Quantity remaining_quantity;
    uint64_t timestamp; // epoch nanoseconds
    Side side;
    OrderType type;
    OrderStatus status;
    
    // Intrusive doubly-linked list pointers for O(1) removal from a PriceLevel
    Order* prev;
    Order* next;

    Order(OrderId id_, Price price_, Quantity qty_, Side side_, OrderType type_, uint64_t ts_)
        : id(id_), price(price_), quantity(qty_), remaining_quantity(qty_), timestamp(ts_),
          side(side_), type(type_), status(OrderStatus::New),
          prev(nullptr), next(nullptr) {}
};

// POD struct for requests flowing through the ring buffer
struct OrderRequest {
    enum class Action : uint8_t { New, Cancel, Modify };
    
    Action action;
    OrderId id;
    Price price;       // Used for New / Modify
    Quantity quantity; // Used for New / Modify
    Side side;
    OrderType type;
    uint64_t timestamp;
};

} // namespace lfe
