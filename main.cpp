#include <atomic>
#include <cstddef>
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
  //   static volatile long long a; // hopefully this isn't optimized away
  while (running.load(std::memory_order_relaxed)) {
    auto a = shitty_fibonacci(40);
  }
}

void *target_thread(void *arg) {
  struct timespec expected, actual;
  static constexpr long INTERVAL_NS = 1000000; // 1 millisecond sleep
  long max_latency_ns = 0;
  static constexpr int MAX_ITERATION = 15000; // Run 5,000 times

  clock_gettime(CLOCK_MONOTONIC, &expected);

  for (int i = 0; i < MAX_ITERATION; i++) {
    expected.tv_nsec += INTERVAL_NS;
    while (expected.tv_nsec >= 1000000000) {
      expected.tv_nsec -= 1000000000;
      expected.tv_sec++;
    }

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &expected, nullptr);
    clock_gettime(CLOCK_MONOTONIC, &actual);

    long curr_latency = (actual.tv_sec - expected.tv_sec) * 1000000000L +
                        (actual.tv_nsec - expected.tv_nsec);

    if (curr_latency > max_latency_ns) {
      max_latency_ns = curr_latency;
    }
  }

  std::cout << "Max Latency Spike: " << max_latency_ns << " ns ("
            << (max_latency_ns / 1000.0) << " us)\n";

  return nullptr;
}

int main(int argc, char *argv[]) {
  bool use_fifo = false;
  if (argc > 1 && std::string(argv[1]) == "--fifo") {
    use_fifo = true;
  }
  std::cout << "Starting measurement (" << (use_fifo ? "SCHED_FIFO" : "Normal")
            << ") \n";

  if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
    std::cerr << "Failed to lock memory: " << std::strerror(errno) << std::endl;
    return 1;
  }

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  if (use_fifo) {

    // disable inheritance (idk if this is necessary )
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    // set policy to FIFO
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    // set prio to max
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
  }

  // pin to core 0
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);   // Clear the cpu set
  CPU_SET(0, &cpuset); // Add core 0 to the set
  pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

  int num_threads = 100; // force some core contention?

  std::cout << "Starting " << num_threads << " noise maker threads\n";
  std::vector<std::thread> noise_threads(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    noise_threads[i] = std::thread(noise_loop);
  }

  usleep(5000000);

  pthread_t thread;
  auto res = pthread_create(&thread, &attr, target_thread, nullptr);
  if (res != 0) {
    std::cout << " failed to create pthread, error code = "
              << std::strerror(res) << std::endl;
    return res;
  }
  pthread_attr_destroy(&attr);

  pthread_join(thread, nullptr);

  running.store(false, std::memory_order_relaxed);
  for (int i = 0; i < num_threads; ++i) {
    noise_threads[i].join();
  }


  return 0;
}