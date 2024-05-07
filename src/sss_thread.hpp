#include "sss_node.hpp"
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <thread>

// TODO:
// BIG CLEANUP

using fn_type = std::function<void()>;

class SSS_Thread {
public:
  SSS_Thread(std::size_t idx, SSS_Node<float> *node) : idx_(idx), node_(node) {}

  void start_thread() {
    thread = std::thread([this] { this->thread_fn_inner(); });
  }

  void thread_fn_inner() {
    for (;;) {
      // std::cout << "thread awake!\n";
      // TODO:
      // progressive backoff here?
      auto res = node_->run_fn();
      sem.acquire();
      // while (res <= 1) {
      //  res = node_->run_fn();
      // }
    }
  }

  void wakeup() { sem.release(); }

private:
  std::size_t idx_;
  // fn_type node_fn_;
  std::thread thread;
  std::counting_semaphore<1> sem{0};
  SSS_Node<float> *node_;
};

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
  // using fn_type = std::function<void()>;
  void start_threads(uint32_t n_threads = std::thread::hardware_concurrency()) {

    running.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < n_threads; ++i) {
      avail_threads.emplace_back([this, i] {
        // fn_type cur_task;
        if (i >= nodes_.size())
          return;
        // cur_task = tasks[i];
        for (;;) {
          // eval_node(i);
          //  std::cout << i << std::endl;
          // sem.acquire();
          //  cur_task();
        }
      });
    }
  }

  SSS_ThreadPool(uint32_t n_threads = std::thread::hardware_concurrency())
      : n_threads(n_threads) {

    tasks_.resize(n_threads);
    nodes_.resize(n_threads);
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

  void signal_all() {
    std::cout << "signaling\n";
    sem.release();
  }

  // BIG TODO:
  //  is a progressive backoff spinlock necessary here?
  //
  // This function iterates through the tasks associated with each
  // thread (stored in thread_task_map), executes the task - or
  // if the task is sequential, traverses the task list and
  // executes in a sequential manner
  void eval_tasks(std::size_t idx) {
    for (auto &t : tasks_[idx]) {
      t();
    }
  }

  void eval_node(std::size_t idx) {
    for (auto &n : nodes_[idx]) {
      auto cur_node = n;
      while (cur_node != nullptr) {
        cur_node->run_fn();
        cur_node = n->next;
      }
    }
  }

  bool get_run_status() { return running.load(std::memory_order_relaxed); }

  template <typename F, typename... Args>
  void push_node_fn_rr(F &&f, Args &&...args) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks_[cur_rr_index].push_back(
          std::bind(std::forward<F>(f), std::forward<Args>(args)...));

      if (cur_rr_index == n_threads)
        cur_rr_index = 0;
      else
        cur_rr_index += 1;
    }
  }

  void push_node_rr(SSS_Node<float> *n) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      nodes_[cur_rr_index].push_back(n);
      std::cout << cur_rr_index << std::endl;

      if (cur_rr_index == n_threads)
        cur_rr_index = 0;
      else
        cur_rr_index += 1;
    }
  }

  // same as push_node_fn_rr but stays on the same index
  template <typename F, typename... Args>
  void push_node_list_fn_rr(F &&f, Args &&...args) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks_[cur_rr_index].push_back(
          std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }
  }

  void increment_rr_index() { cur_rr_index += 1; }

  void signal_threads() {
    // std::cout << "signaling threads!\n";
    for (std::size_t i = 0; i < threads_.size(); i++)
      threads_[i]->wakeup();
    // std::cout << "thread wakeup\n";
    //  sem.acquire();
  }

  void signal_in_threads() {
    // std::cout << "signaling threads!\n";
    for (std::size_t i = 0; i < in_threads_.size(); i++)
      in_threads_[i]->wakeup();
    // std::cout << "thread wakeup\n";
    //  sem.acquire();
  }

  void register_thread(SSS_Node<float> *node, bool input = false) {
    auto thread = new SSS_Thread(cur_rr_index++, node);
    if (cur_rr_index == n_threads)
      cur_rr_index = 0;
    else
      cur_rr_index += 1;

    thread->start_thread();
    if (input)
      in_threads_.push_back(thread);
    else
      threads_.push_back(thread);
  }

private:
  std::deque<fn_type> tasks;
  std::vector<std::vector<fn_type>> tasks_;
  std::vector<std::vector<SSS_Node<float> *>> nodes_;
  std::deque<std::thread> avail_threads;
  std::deque<SSS_Thread *> threads_;
  std::deque<SSS_Thread *> in_threads_;
  std::mutex queue_mutex;
  std::condition_variable condition;
  std::atomic<bool> running;
  std::counting_semaphore<1> sem{0}; // TODO: each thread needs it's own sem
  std::size_t n_threads;
  // current index for round-robin assigning
  std::size_t cur_rr_index{0};

  bool stop;
};
