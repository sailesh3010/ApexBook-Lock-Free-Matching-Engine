#include <gtest/gtest.h>
#include "engine/matching_engine.h"
#include "concurrency/spsc_queue.h"

using namespace lfe;

class MatchingEngineTest : public ::testing::Test {
protected:
    MatchingEngineTest() : egress(1024), engine(1000, egress) {}

    SPSCQueue<OutboundMessage> egress;
    MatchingEngine engine;
};

TEST_F(MatchingEngineTest, TestLimitOrderInsert) {
    OrderRequest req{OrderRequest::Action::New, 1, 100, 50, Side::Buy, OrderType::Limit, 1000};
    engine.process(req);
    
    // Process egress
    OutboundMessage msg;
    bool has_msg = egress.dequeue(msg);
    EXPECT_TRUE(has_msg);
    EXPECT_EQ(msg.type, OutboundMessage::Type::BookUpdate);
    EXPECT_EQ(msg.book_update.price, 100);
    EXPECT_EQ(msg.book_update.new_volume, 50);
}

TEST_F(MatchingEngineTest, TestMatching) {
    OrderRequest buy{OrderRequest::Action::New, 1, 100, 50, Side::Buy, OrderType::Limit, 1000};
    engine.process(buy);
    
    // Clear egress
    OutboundMessage msg;
    while(egress.dequeue(msg));

    OrderRequest sell{OrderRequest::Action::New, 2, 100, 50, Side::Sell, OrderType::Limit, 1001};
    engine.process(sell);

    // We expect a Trade event and OrderState events
    bool trade_found = false;
    while(egress.dequeue(msg)) {
        if (msg.type == OutboundMessage::Type::Trade) {
            trade_found = true;
            EXPECT_EQ(msg.trade.match_price, 100);
            EXPECT_EQ(msg.trade.match_quantity, 50);
            EXPECT_EQ(msg.trade.aggressive_order_id, 2);
            EXPECT_EQ(msg.trade.resting_order_id, 1);
        }
    }
    EXPECT_TRUE(trade_found);
}

TEST_F(MatchingEngineTest, TestPartialFill) {
    OrderRequest buy{OrderRequest::Action::New, 1, 100, 100, Side::Buy, OrderType::Limit, 1000};
    engine.process(buy);
    
    OrderRequest sell{OrderRequest::Action::New, 2, 100, 40, Side::Sell, OrderType::Limit, 1001};
    engine.process(sell);

    bool trade_found = false;
    OutboundMessage msg;
    while(egress.dequeue(msg)) {
        if (msg.type == OutboundMessage::Type::Trade) {
            trade_found = true;
            EXPECT_EQ(msg.trade.match_quantity, 40);
        }
    }
    EXPECT_TRUE(trade_found);
}

TEST_F(MatchingEngineTest, TestCancelOrder) {
    OrderRequest buy{OrderRequest::Action::New, 1, 100, 50, Side::Buy, OrderType::Limit, 1000};
    engine.process(buy);

    OrderRequest cancel{OrderRequest::Action::Cancel, 1, 0, 0, Side::Buy, OrderType::Limit, 1001};
    engine.process(cancel);

    bool cancel_found = false;
    OutboundMessage msg;
    while(egress.dequeue(msg)) {
        if (msg.type == OutboundMessage::Type::OrderState && msg.order_state.status == OrderStatus::Cancelled) {
            cancel_found = true;
            EXPECT_EQ(msg.order_state.id, 1);
        }
    }
    EXPECT_TRUE(cancel_found);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
