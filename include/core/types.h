#pragma once

#include <cstdint>
#include <limits>

namespace lfe { // Lock-Free Exchange

// Use fixed-width integers for absolute size guarantees
using Price = uint64_t;
using Quantity = uint32_t;
using OrderId = uint64_t;

// Special constants
constexpr Price INVALID_PRICE = std::numeric_limits<Price>::max();
constexpr OrderId INVALID_ORDER_ID = 0;

enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t {
    Market = 0,
    Limit = 1
};

enum class OrderStatus : uint8_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    Rejected = 4
};

} // namespace lfe
