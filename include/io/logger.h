#pragma once

#include "concurrency/mpsc_queue.h"
#include <string>
#include <thread>
#include <fstream>
#include <iostream>
#include <cstring>

namespace lfe {

class AsyncLogger {
public:
    enum class Level { Info, Warn, Error };

    struct LogMessage {
        Level level;
        char data[256]; // Fixed size to avoid dynamic allocations
    };

    explicit AsyncLogger(const std::string& filename, size_t queue_capacity = 8192)
        : queue_(queue_capacity), running_(true) {
        if (!filename.empty()) {
            file_out_.open(filename, std::ios::app);
        }
        worker_ = std::thread(&AsyncLogger::process_logs, this);
    }

    ~AsyncLogger() {
        running_.store(false, std::memory_order_release);
        if (worker_.joinable()) {
            worker_.join();
        }
        if (file_out_.is_open()) {
            file_out_.close();
        }
    }

    // Call from any thread, lock-free
    void log(Level level, const char* msg) {
        LogMessage lm;
        lm.level = level;
        std::strncpy(lm.data, msg, sizeof(lm.data) - 1);
        lm.data[sizeof(lm.data) - 1] = '\0';
        queue_.enqueue(lm);
    }

private:
    void process_logs() {
        LogMessage msg;
        while (running_.load(std::memory_order_acquire) || queue_.dequeue(msg)) {
            while (queue_.dequeue(msg)) {
                write(msg);
            }
            std::this_thread::yield(); // simple backoff
        }
    }

    void write(const LogMessage& msg) {
        const char* prefix = "[INFO] ";
        if (msg.level == Level::Warn) prefix = "[WARN] ";
        if (msg.level == Level::Error) prefix = "[ERROR] ";

        if (file_out_.is_open()) {
            file_out_ << prefix << msg.data << "\n";
        } else {
            std::cout << prefix << msg.data << "\n";
        }
    }

    MPSCQueue<LogMessage> queue_;
    std::atomic<bool> running_;
    std::thread worker_;
    std::ofstream file_out_;
};

} // namespace lfe
