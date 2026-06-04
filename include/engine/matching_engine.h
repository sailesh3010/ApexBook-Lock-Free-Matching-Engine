#pragma once

#include "core/order.h"
#include "core/messages.h"
#include "engine/order_book.h"
#include "memory/object_pool.h"
#include "concurrency/spsc_queue.h"

namespace lfe {

class MatchingEngine {
public:
    MatchingEngine(size_t max_orders, SPSCQueue<OutboundMessage>& egress_queue)
        : order_book_(max_orders), pool_(max_orders), egress_queue_(egress_queue) {}

    // Process an incoming order request from the ingress lock-free queue
    void process(const OrderRequest& req) {
        switch (req.action) {
            case OrderRequest::Action::New:
                handle_new_order(req);
                break;
            case OrderRequest::Action::Cancel:
                handle_cancel_order(req.id);
                break;
            case OrderRequest::Action::Modify:
                handle_modify_order(req);
                break;
        }
    }

private:
    void handle_new_order(const OrderRequest& req) {
        Order* order = pool_.allocate(req.id, req.price, req.quantity, req.side, req.type, req.timestamp);
        if (!order) {
            // Pool exhausted
            egress_queue_.enqueue(OutboundMessage::make_order_state(req.id, OrderStatus::Rejected, req.quantity));
            return;
        }

        match_order(order);

        if (order->remaining_quantity > 0) {
            if (order->type == OrderType::Limit) {
                order_book_.add_order(order);
                publish_book_update(order->price, order->side);
            } else {
                // Market order unfilled part is cancelled
                order->status = OrderStatus::Cancelled;
                egress_queue_.enqueue(OutboundMessage::make_order_state(order->id, order->status, 0));
                pool_.release(order);
            }
        }
    }

    void handle_cancel_order(OrderId id) {
        Order* order = order_book_.get_order(id);
        if (!order) return;

        Price px = order->price;
        Side side = order->side;

        order_book_.cancel_order(id);
        egress_queue_.enqueue(OutboundMessage::make_order_state(id, OrderStatus::Cancelled, order->remaining_quantity));
        publish_book_update(px, side);
        pool_.release(order);
    }

    void handle_modify_order(const OrderRequest& req) {
        Order* order = order_book_.get_order(req.id);
        if (!order) return;

        if (req.price == order->price && req.quantity < order->remaining_quantity) {
            // Price same, quantity reduced -> Keep priority
            order_book_.reduce_order(req.id, req.quantity);
            publish_book_update(order->price, order->side);
            egress_queue_.enqueue(OutboundMessage::make_order_state(req.id, order->status, order->remaining_quantity));
        } else {
            // Price changed or quantity increased -> Lose priority (Cancel and Replace)
            // Save properties to recreate
            Side side = order->side;
            OrderType type = order->type;
            
            // Cancel original
            handle_cancel_order(req.id);

            // Insert new as replacement (re-using ID)
            OrderRequest new_req;
            new_req.action = OrderRequest::Action::New;
            new_req.id = req.id;
            new_req.price = req.price;
            new_req.quantity = req.quantity;
            new_req.side = side;
            new_req.type = type;
            new_req.timestamp = req.timestamp;
            
            handle_new_order(new_req);
        }
    }

    void match_order(Order* aggressive_order) {
        bool is_buy = (aggressive_order->side == Side::Buy);

        while (aggressive_order->remaining_quantity > 0) {
            PriceLevel* best_level = is_buy ? order_book_.get_best_ask_level() : order_book_.get_best_bid_level();
            
            if (!best_level) break; // Book is empty on the opposite side
            
            Price best_price = is_buy ? order_book_.best_ask() : order_book_.best_bid();

            // Price check for limit orders
            if (aggressive_order->type == OrderType::Limit) {
                if (is_buy && aggressive_order->price < best_price) break;
                if (!is_buy && aggressive_order->price > best_price) break;
            }

            Order* resting_order = best_level->front();
            while (resting_order && aggressive_order->remaining_quantity > 0) {
                Quantity match_qty = std::min(aggressive_order->remaining_quantity, resting_order->remaining_quantity);
                Price match_price = resting_order->price; // Resting order determines execution price

                // Execute trade
                aggressive_order->remaining_quantity -= match_qty;
                resting_order->remaining_quantity -= match_qty;
                best_level->reduce_volume(match_qty);

                aggressive_order->status = (aggressive_order->remaining_quantity == 0) ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
                resting_order->status = (resting_order->remaining_quantity == 0) ? OrderStatus::Filled : OrderStatus::PartiallyFilled;

                // Emit trade event
                egress_queue_.enqueue(OutboundMessage::make_trade(
                    aggressive_order->id, resting_order->id, match_price, match_qty, aggressive_order->timestamp
                ));

                Order* next_resting = resting_order->next;

                // If resting is fully filled, remove it
                if (resting_order->remaining_quantity == 0) {
                    order_book_.remove_from_book(resting_order);
                    order_book_.clear_registry(resting_order->id);
                    egress_queue_.enqueue(OutboundMessage::make_order_state(resting_order->id, OrderStatus::Filled, 0));
                    pool_.release(resting_order);
                } else {
                    // It was partially filled, emit update
                    egress_queue_.enqueue(OutboundMessage::make_order_state(resting_order->id, OrderStatus::PartiallyFilled, resting_order->remaining_quantity));
                }

                resting_order = next_resting;
            }

            // Publish book update after sweeping the level
            publish_book_update(best_price, is_buy ? Side::Sell : Side::Buy);
        }

        // Emit aggressive order state if filled or partially filled before resting
        if (aggressive_order->status == OrderStatus::Filled || aggressive_order->status == OrderStatus::PartiallyFilled) {
             egress_queue_.enqueue(OutboundMessage::make_order_state(aggressive_order->id, aggressive_order->status, aggressive_order->remaining_quantity));
        }
    }

    void publish_book_update(Price px, Side side) {
        Quantity vol = order_book_.volume_at(px, side);
        egress_queue_.enqueue(OutboundMessage::make_book_update(px, vol, side));
    }

    OrderBook order_book_;
    ObjectPool<Order> pool_;
    SPSCQueue<OutboundMessage>& egress_queue_;
};

} // namespace lfe
