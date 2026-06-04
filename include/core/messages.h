#pragma once

#include "core/types.h"
#include <cstdint>

namespace lfe {

struct TradeEvent {
    OrderId aggressive_order_id;
    OrderId resting_order_id;
    Price match_price;
    Quantity match_quantity;
    uint64_t timestamp;
};

struct OrderStateEvent {
    OrderId id;
    OrderStatus status;
    Quantity remaining_quantity;
};

struct BookUpdateEvent {
    Price price;
    Quantity new_volume;
    Side side;
};

// Represents an event output from the Matching Engine to the egress queues
struct OutboundMessage {
    enum class Type : uint8_t {
        Trade,
        OrderState,
        BookUpdate
    };

    Type type;
    union {
        TradeEvent trade;
        OrderStateEvent order_state;
        BookUpdateEvent book_update;
    };

    OutboundMessage() = default;

    // Helper constructors
    static OutboundMessage make_trade(OrderId agg_id, OrderId rest_id, Price px, Quantity qty, uint64_t ts) {
        OutboundMessage msg;
        msg.type = Type::Trade;
        msg.trade = {agg_id, rest_id, px, qty, ts};
        return msg;
    }

    static OutboundMessage make_order_state(OrderId id, OrderStatus status, Quantity rem_qty) {
        OutboundMessage msg;
        msg.type = Type::OrderState;
        msg.order_state = {id, status, rem_qty};
        return msg;
    }

    static OutboundMessage make_book_update(Price px, Quantity vol, Side side) {
        OutboundMessage msg;
        msg.type = Type::BookUpdate;
        msg.book_update = {px, vol, side};
        return msg;
    }
};

} // namespace lfe
