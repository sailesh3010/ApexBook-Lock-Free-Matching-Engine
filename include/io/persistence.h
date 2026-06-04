#pragma once

#include "core/order.h"
#include <fstream>
#include <vector>
#include <string>

namespace lfe {

class Persistence {
public:
    explicit Persistence(const std::string& journal_path) 
        : journal_path_(journal_path) {
    }

    void open_for_write() {
        journal_.open(journal_path_, std::ios::binary | std::ios::app);
    }

    void append_transaction(const OrderRequest& req) {
        if (journal_.is_open()) {
            journal_.write(reinterpret_cast<const char*>(&req), sizeof(OrderRequest));
            // In high-performance systems, we don't flush every time unless hardware guarantees it
        }
    }

    void close() {
        if (journal_.is_open()) {
            journal_.flush();
            journal_.close();
        }
    }

    // Recovers all transactions from the journal
    std::vector<OrderRequest> recover() {
        std::vector<OrderRequest> transactions;
        std::ifstream in(journal_path_, std::ios::binary);
        if (!in.is_open()) return transactions;

        OrderRequest req;
        while (in.read(reinterpret_cast<char*>(&req), sizeof(OrderRequest))) {
            transactions.push_back(req);
        }
        return transactions;
    }

private:
    std::string journal_path_;
    std::ofstream journal_;
};

} // namespace lfe
