#pragma once

#include <optional>
#include <pthread.h>

namespace rt {
struct Settings {
  bool fifo{false};
  std::optional<int> priority{std::nullopt};
  std::optional<int> core{std::nullopt};
};

class Thread {
public:
  Thread() : joinable_(false) {}
  Thread(Thread &&other) {
    this->joinable_ = other.joinable_;
    this->thread_ = other.thread_;
    other.joinable_ = false;
  }
  Thread(const Thread &other) = delete;
  Thread &operator=(Thread &&other) {
    thread_ = other.thread_;
    joinable_ = other.joinable_;
    other.joinable_ = false;
    return *this;
  }
  Thread &operator=(const Thread &other) = delete;

  template <typename Callable, typename... Args>
  Thread(Callable &&thread_fn, const Settings &settings = {}, Args... args) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    if (settings.fifo) {
      pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
      pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

      if (settings.priority.has_value()) {
        struct sched_param param;
        param.sched_priority = settings.priority.value();
        pthread_attr_setschedparam(&attr, &param);
      }

      if (settings.core.has_value()) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(settings.core.value(), &cpuset);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
      }
    }

    int res = pthread_create(&thread_, nullptr, thread_fn, args...);
  }

  ~Thread() {
    if (joinable()) {
      join();
    }
  }
  bool joinable() { return joinable_; }

  void join() {
    pthread_join(thread_, nullptr);
    joinable_ = false;
  }

private:
  bool joinable_{true};
  pthread_t thread_;
};

} // namespace rt