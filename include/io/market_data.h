#pragma once

#include "core/messages.h"
#include <iostream>
#include <iomanip>

namespace lfe {

class MarketDataPublisher {
public:
    MarketDataPublisher() = default;

    // Call from egress consumer thread
    void process_message(const OutboundMessage& msg) {
        switch (msg.type) {
            case OutboundMessage::Type::Trade:
                update_last_trade(msg.trade);
                break;
            case OutboundMessage::Type::BookUpdate:
                update_bbo(msg.book_update);
                break;
            case OutboundMessage::Type::OrderState:
                // Typically used for private drop copies, but can be ignored for public L2
                break;
        }
    }

    void update_last_trade(const TradeEvent& trade) {
        last_trade_price_ = trade.match_price;
        total_volume_ += trade.match_quantity;
    }

    void update_bbo(const BookUpdateEvent& update) {
        // Simplified BBO tracker
        if (update.side == Side::Buy) {
            if (update.new_volume == 0 && update.price == best_bid_) {
                // Best bid might have moved down, we wait for next update to reflect it
                // Or we require the OrderBook to explicitly emit a "New Best Bid" event
                best_bid_ = 0; 
                best_bid_qty_ = 0;
            } else if (update.price >= best_bid_) {
                best_bid_ = update.price;
                best_bid_qty_ = update.new_volume;
            }
        } else {
            if (update.new_volume == 0 && update.price == best_ask_) {
                best_ask_ = INVALID_PRICE;
                best_ask_qty_ = 0;
            } else if (update.price <= best_ask_ || best_ask_ == INVALID_PRICE) {
                best_ask_ = update.price;
                best_ask_qty_ = update.new_volume;
            }
        }
    }

    void print_snapshot() const {
        std::cout << "--- Market Data Snapshot ---\n";
        std::cout << "Best Bid: " << best_bid_ << " @ " << best_bid_qty_ << "\n";
        std::cout << "Best Ask: " << (best_ask_ == INVALID_PRICE ? 0 : best_ask_) << " @ " << best_ask_qty_ << "\n";
        std::cout << "Last Trade: " << last_trade_price_ << "\n";
        std::cout << "Total Volume: " << total_volume_ << "\n";
        std::cout << "----------------------------\n";
    }

private:
    Price best_bid_{0};
    Quantity best_bid_qty_{0};
    Price best_ask_{INVALID_PRICE};
    Quantity best_ask_qty_{0};
    Price last_trade_price_{0};
    Quantity total_volume_{0};
};

} // namespace lfe
