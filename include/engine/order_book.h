#pragma once

#include "core/order.h"
#include "engine/price_level.h"
#include <vector>
#include <map>
#include <functional>

namespace lfe {

// The Limit Order Book
// Assumes a sparse price range for simplicity using std::map, but with an Order Registry for O(1) lookup.
// In a highly optimized specific-asset exchange, the maps would be replaced with flat arrays indexed by ticks.
class OrderBook {
public:
    explicit OrderBook(size_t max_orders) {
        order_registry_.resize(max_orders, nullptr);
    }

    ~OrderBook() = default;

    // Registers and adds the order to the book
    void add_order(Order* order) {
        // Register for O(1) lookup
        if (order->id < order_registry_.size()) {
            order_registry_[order->id] = order;
        } else {
            // Unlikely in preallocated dense ID scenario, but safe fallback
            if (order->id >= order_registry_.size()) {
                order_registry_.resize(order->id + 10000, nullptr);
            }
            order_registry_[order->id] = order;
        }

        // Add to Price Level
        if (order->side == Side::Buy) {
            bids_[order->price].append(order);
            if (order->price > best_bid_) {
                best_bid_ = order->price;
            }
        } else {
            asks_[order->price].append(order);
            if (order->price < best_ask_) {
                best_ask_ = order->price;
            }
        }
    }

    // O(1) order cancellation using the registry
    void cancel_order(OrderId id) {
        Order* order = get_order(id);
        if (!order) return;

        remove_from_book(order);
        order->status = OrderStatus::Cancelled;
        order_registry_[id] = nullptr;
    }

    // O(1) order modification (quantity reduction only to keep priority)
    // If price changes or quantity increases, it loses priority (implemented by engine via cancel-replace)
    void reduce_order(OrderId id, Quantity new_quantity) {
        Order* order = get_order(id);
        if (!order || new_quantity >= order->remaining_quantity) return;
        
        Quantity reduction = order->remaining_quantity - new_quantity;
        order->remaining_quantity = new_quantity;

        if (order->side == Side::Buy) {
            bids_[order->price].reduce_volume(reduction);
        } else {
            asks_[order->price].reduce_volume(reduction);
        }
    }

    Order* get_order(OrderId id) const {
        if (id < order_registry_.size()) {
            return order_registry_[id];
        }
        return nullptr;
    }

    void remove_from_book(Order* order) {
        if (order->side == Side::Buy) {
            auto it = bids_.find(order->price);
            if (it != bids_.end()) {
                it->second.remove(order);
                if (it->second.empty()) {
                    bids_.erase(it);
                    update_best_bid();
                }
            }
        } else {
            auto it = asks_.find(order->price);
            if (it != asks_.end()) {
                it->second.remove(order);
                if (it->second.empty()) {
                    asks_.erase(it);
                    update_best_ask();
                }
            }
        }
    }

    // Accessors
    Price best_bid() const { return best_bid_; }
    Price best_ask() const { return best_ask_; }

    PriceLevel* get_best_bid_level() {
        if (bids_.empty()) return nullptr;
        return &bids_.rbegin()->second; // Map is sorted ascending, rbegin is highest price
    }

    PriceLevel* get_best_ask_level() {
        if (asks_.empty()) return nullptr;
        return &asks_.begin()->second; // Map is sorted ascending, begin is lowest price
    }

    void remove_best_bid_level() {
        if (!bids_.empty()) {
            bids_.erase(std::prev(bids_.end()));
            update_best_bid();
        }
    }

    void remove_best_ask_level() {
        if (!asks_.empty()) {
            asks_.erase(asks_.begin());
            update_best_ask();
        }
    }
    
    // Clear an order from the registry (when filled or fully cancelled)
    void clear_registry(OrderId id) {
        if (id < order_registry_.size()) {
            order_registry_[id] = nullptr;
        }
    }

    Quantity volume_at(Price price, Side side) const {
        if (side == Side::Buy) {
            auto it = bids_.find(price);
            return (it != bids_.end()) ? it->second.volume() : 0;
        } else {
            auto it = asks_.find(price);
            return (it != asks_.end()) ? it->second.volume() : 0;
        }
    }

private:
    void update_best_bid() {
        if (bids_.empty()) {
            best_bid_ = 0;
        } else {
            best_bid_ = bids_.rbegin()->first;
        }
    }

    void update_best_ask() {
        if (asks_.empty()) {
            best_ask_ = INVALID_PRICE;
        } else {
            best_ask_ = asks_.begin()->first;
        }
    }

    // We use std::map (Red-Black tree) here to support unbounded sparse price ranges.
    // For ultimate HFT latency, this would be an array (if bounded tick range) or a highly-optimized flat b-tree.
    // Map of price -> PriceLevel
    std::map<Price, PriceLevel> bids_;
    std::map<Price, PriceLevel> asks_;

    Price best_bid_{0};
    Price best_ask_{INVALID_PRICE};

    // O(1) order lookup registry by ID
    std::vector<Order*> order_registry_;
};

} // namespace lfe
