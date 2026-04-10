#pragma once

#include <cstring>
#include <iostream>
#include <optional>
#include <pthread.h>
#include <sys/mman.h>
#include <tuple>
#include <type_traits>

namespace rt {
struct Settings {
  bool fifo{false};
  std::optional<int> priority{std::nullopt};
  std::optional<int> core{std::nullopt};
};

template <typename Callable, typename... Args> struct ThreadPayload {
  Callable callable_;
  std::tuple<Args...> args_;

  ThreadPayload(Callable &&fn, Args &&...args)
      : callable_(fn), args_(args...) {}

  void invoke() { std::apply(callable_, args_); } // this calls the function
};

template <typename Callable, typename... Args>
void *pthread_fn(void *payload) { // this takes ownership of the payload
  auto *thread_payload =
      static_cast<ThreadPayload<Callable, Args...> *>(payload);
  thread_payload->invoke();
  delete thread_payload;
  return nullptr;
}

// joining thread
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
      if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        std::cerr << "Failed to lock memory: " << std::strerror(errno)
                  << std::endl;
        // just ignore for now
      }
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

    auto *payload = new ThreadPayload<Callable, Args...>(
        std::decay_t<Callable>(thread_fn), std::decay_t<Args>(args)...);

    int res = pthread_create(&thread_, nullptr, &pthread_fn<std::decay_t<Callable>,
                                                             std::decay_t<Args>...>,
                             payload);

    joinable_ = true;
  }

  ~Thread() {
    if (joinable()) {
      // pthread_detach(thread_);
      join();
    }
  }
  bool joinable() { return joinable_; }

  void join() {
    if (joinable_) {
      pthread_join(thread_, nullptr);
      joinable_ = false;
    }
  }

private:
  bool joinable_{false};

  pthread_t thread_{}; // this is just an id so can be default initialized.
};

} // namespace rt