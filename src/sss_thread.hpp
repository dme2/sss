#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <thread>

struct spinlock {
  std::atomic<bool> lock_ = {0};

  void lock() noexcept {
    for (;;) {
      // Optimistically assume the lock is free on the first try
      if (!lock_.exchange(true, std::memory_order_acquire)) {
        return;
      }
      // Wait for lock to be released without generating cache misses
      while (lock_.load(std::memory_order_relaxed)) {
        // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
        // hyper-threads
        // __builtin_ia32_pause();
        __asm__ volatile("yield" ::: "memory");
      }
    }
  }

  bool try_lock() noexcept {
    // First do a relaxed load to check if lock is free in order to prevent
    // unnecessary cache misses if someone does while(!try_lock())
    return !lock_.load(std::memory_order_relaxed) &&
           !lock_.exchange(true, std::memory_order_acquire);
  }

  void unlock() noexcept { lock_.store(false, std::memory_order_release); }
};

class SSS_ThreadPool {
public:
  using fn_type = std::function<void()>;
  void start_threads(uint32_t n_threads = std::thread::hardware_concurrency()) {

    running.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < n_threads; ++i) {
      avail_threads.emplace_back([this, i] {
        fn_type cur_task;
        // while (true) {
        //   {
        //    std::unique_lock<std::mutex> lock(spin_lock);
        //   condition.wait(lock, [this] { return stop || !tasks.empty(); });
        //  if (stop && tasks.empty())
        //   return;
        //  }
        //  cur_task = std::move(tasks.front());
        //  tasks.pop_front();
        //  cur_task();
        //  spin_lock.unlock();
        // std::cout << "unlocked\n";
        if (i >= tasks.size())
          return;
        cur_task = tasks[i];
        for (;;) {
          cur_task();
          // TODO: replace with a spinlock
          sem.acquire();
        }
        //}
      });
    }
  }

  SSS_ThreadPool(uint32_t n_threads = std::thread::hardware_concurrency()) {
    /*
        for (std::size_t i = 0; i < n_threads; ++i) {
          avail_threads.emplace_back([this, i] {
            fn_type cur_task;
            //while (true) {
              //  {
              //   std::unique_lock<std::mutex> lock(spin_lock);
              //  condition.wait(lock, [this] { return stop || !tasks.empty();
       });
              // if (stop && tasks.empty())
              //  return;
              // }
              // cur_task = std::move(tasks.front());
              // tasks.pop_front();
              // cur_task();
              // spin_lock.unlock();
              //std::cout << "unlocked\n";
              if (i >= tasks.size())
                return;
              cur_task = tasks[i];
              for (;;)
                cur_task();
                //}
          });
        }
        */
  }

  ~SSS_ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop = true;
    }

    condition.notify_all();
    for (std::thread &t : avail_threads) {
      t.join();
    }
  }

  template <typename F, typename... Args> void enqueue(F &&f, Args &&...args) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks.push_back(
          std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }
    condition.notify_one();
  }

  template <typename F, typename... Args>
  void push_node_tp(F &&f, Args &&...args) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks.push_back(
          std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }
  }
  void signal_all() { sem.release(); }

  // BIG TODO:
  //  is a progressive backoff spinlock necessary here?
  // This function iterates through the tasks associated with each
  // thread (stored in thread_task_map), executes the task - or
  // if the task is sequential, traverses the task list and
  // executes in a sequential manner
  void eval_task() {}

  bool get_run_status() { return running.load(std::memory_order_relaxed); }

private:
  std::deque<fn_type> tasks;
  std::deque<std::thread> avail_threads;
  std::mutex queue_mutex;
  std::condition_variable condition;
  std::atomic<bool> running;
  std::counting_semaphore<1> sem{0};

  bool stop;
};
