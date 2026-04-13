#include "publisher.hpp"
// #include "queue.hpp"
#include "thread.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <endian.h>
#include <iostream>
#include <pthread.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>

// random expensive operation
long long shitty_fibonacci(int i) {
    if (i <= 1) {
        return i;
    }
    return shitty_fibonacci(i - 1) + shitty_fibonacci(i - 2);
}

std::atomic_bool running(true);
// just to nuke the cpu
void noise_loop() {
    static volatile long long a; // hopefully this isn't optimized away
    while (running.load(std::memory_order_relaxed)) {
        auto a = shitty_fibonacci(40);
    }
}

void *target_thread(void *arg) {
    struct timespec       expected, actual;
    static constexpr long INTERVAL_NS    = 1000000; // 1 millisecond sleep
    long                  max_latency_ns = 0;
    static constexpr int  MAX_ITERATION  = 15000; // Run 5,000 times

    clock_gettime(CLOCK_MONOTONIC, &expected);

    for (int i = 0; i < MAX_ITERATION; i++) {
        expected.tv_nsec += INTERVAL_NS;
        while (expected.tv_nsec >= 1000000000) {
            expected.tv_nsec -= 1000000000;
            expected.tv_sec++;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &expected, nullptr);
        clock_gettime(CLOCK_MONOTONIC, &actual);

        long curr_latency = (actual.tv_sec - expected.tv_sec) * 1000000000L + (actual.tv_nsec - expected.tv_nsec);

        if (curr_latency > max_latency_ns) {
            max_latency_ns = curr_latency;
        }
    }

    std::cout << "Max Latency Spike: " << max_latency_ns << " ns (" << (max_latency_ns / 1000.0) << " us)\n";

    return nullptr;
}

int main(int argc, char *argv[]) {
    bool use_fifo = false;
    if (argc > 1 && std::string(argv[1]) == "--fifo") {
        use_fifo = true;
    }
    std::cout << "Starting measurement (" << (use_fifo ? "SCHED_FIFO" : "Normal") << ") \n";
    lfq::Handle handle;

    rt::Thread publisher_thread(
        [&handle]() {
            lfq::Publisher publisher(handle);
            int            i = 0;
            while (running.load(std::memory_order_relaxed)) {
                publisher.Publish(i++);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        },
        {.fifo = use_fifo, .priority = 100, .core = 0});

    lfq::Subscriber subscriber(handle, [](int item) { std::cout << item << "\n"; },
                               {.fifo = use_fifo, .priority = 100, .core = 1});

    std::getchar();

    return 0;
}