#pragma once
#include "queue.hpp"
#include "thread.hpp"

#include <atomic>
#include <thread>

namespace lfq {

struct Handle {
    SimpleQueue<int> queue;
};

class Publisher {
  public:
    Publisher(Handle &handle) : handle_(handle) {
    }

    bool Publish(int item) {
        return handle_.queue.push(item);
    }

  private:
    Handle &handle_;
};

class Subscriber {
  public:
    Subscriber(Handle &handle, void (*callback)(int), const rt::Settings &settings = {})
        : handle_(handle), callback_(callback) {
        thread_ = rt::Thread([this]() {
            while (running_.test(std::memory_order_acquire)) {
                int item;
                if (handle_.queue.pop(item)) {
                    callback_(item);
                }
            }
        },
        settings);
    }

    ~Subscriber() {
        running_.clear(std::memory_order_release);
        thread_.join();
    }

  private:
    std::atomic_flag running_{true};
    void (*callback_)(int);
    rt::Thread thread_;
    Handle &handle_;
};
} // namespace lfq