#pragma once

#include <atomic>
#include <cstddef>

namespace lfq {

template <typename T, size_t N = 512>
class SimpleQueue {
  public:
    bool push(const T &item) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        size_t next_head    = (current_head + 1) % N;
        if (next_head == tail_.load(std::memory_order_acquire)) {
            // queue is full
            return false;
        }

        buffer_[current_head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool pop(T &item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == current_tail) {
            // queue is empty
            return false;
        }

        item = buffer_[current_tail];
        tail_.store((current_tail + 1) % N, std::memory_order_release);
        return true;
    }

  private:
    alignas(64) std::atomic_size_t head_{0}; // producer
    alignas(64) std::atomic_size_t tail_{0}; // consumer
    T buffer_[N];
};

} // namespace lfq