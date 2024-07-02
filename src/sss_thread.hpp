// #include "sss_node.hpp"
#include "sss_msg_queue.hpp" // includes sss_node
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <thread>
#include <unordered_map>

using fn_type = std::function<void()>;

class SSS_Thread {
public:
  SSS_Thread(std::size_t idx, SSS_NodeECS<MAX_NODES> *ecs,
             SSS_Msg_Queue *msg_queue, SSS_Node<float> *node = nullptr)
      : idx_(idx), node_(node), node_ecs_(ecs), msg_queue_(msg_queue) {
    node_list_ = new SSS_NodeList<float>();
  }

  int backoff{0};

  void start_thread() {
    thread = std::thread([this] { this->thread_fn_inner(); });
  }

  void thread_fn_inner() {
    for (;;) {

      backoff = 0;

      sem.acquire();
      while (pop_and_run_node()) {
        backoff++;
      }
    }
  }

  bool pop_and_run_node() {
    auto msg = msg_queue_->pop_msg();
    if (msg.has_value()) {
      auto node = node_ecs_->node_ecs[msg->node_idx];
      node->run_fn();
      return true;
    } else
      return false;
  }

  void run_assigned_nodes() {
    for (size_t i = 0; i < num_nodes; i++) {
      auto n = this->node_ecs_->node_ecs[ecs_handles_[i]];
      n->run_fn();
    }
  }

  void run_assigned_nodes_dev() {
    for (size_t i = 0; i < num_nodes; i++) {
      auto n = this->node_ecs_->node_ecs[ecs_handles_[i]];
      n->run_fn();
    }
  }

  void wakeup() { sem.release(); }

  void set_node(SSS_Node<float> *node) { this->node_ = node; }

  void push_node(SSS_Node<float> *node) {
    this->nodes_.push_back(node);
    num_nodes += 1;
  }

  void push_node_ecs(int idx) {
    this->ecs_handles_.push_back(idx);
    num_nodes += 1;
  }

  SSS_NodeECS<MAX_NODES> *node_ecs_;
  SSS_Msg_Queue *msg_queue_;

  uint32_t cur_id{0};

private:
  std::size_t idx_;

  // indices into the ecs array
  std::vector<size_t> ecs_handles_;
  std::unordered_map<uint32_t, size_t> device_ecs_handles_;
  uint32_t device_id{0};
  // fn_type node_fn_;
  std::thread thread;
  std::counting_semaphore<1> sem{0};
  SSS_Node<float> *node_;

  SSS_NodeList<float> *node_list_;
  std::vector<SSS_Node<float> *> nodes_;
  std::size_t num_nodes{0};
};

class spinlock {
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
  /*
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
  */

  SSS_ThreadPool(std::size_t n_out_threads, std::size_t n_in_threads,
                 SSS_NodeECS<MAX_NODES> *ecs, SSS_Msg_Queue *msg_queue)
      : n_out_threads(n_out_threads) {

    for (int i = 0; i < n_out_threads; i++) {
      auto new_thread = new SSS_Thread(i, ecs, msg_queue);
      // new_thread->node_ecs_ = this->node_ecs_;
      threads_.push_back(new_thread);
      new_thread->start_thread();
    }

    for (int i = 0; i < n_in_threads; i++) {
      auto new_thread = new SSS_Thread(i, ecs, msg_queue);
      // new_thread->node_ecs_ = this->node_ecs_;
      in_threads_.push_back(new_thread);
      new_thread->start_thread();
    }
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
    // std::cout << "signaling\n";
    sem.release();
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

  void increment_rr_index() { cur_rr_index += 1; }

  void signal_threads(uint32_t device_id) {
    for (std::size_t i = 0; i < threads_.size(); i++) {
      // threads_[i]->cur_id = device_id;
      threads_[i]->wakeup();
    }
  }

  void signal_in_threads(uint32_t device_id) {
    for (std::size_t i = 0; i < in_threads_.size(); i++) {
      in_threads_[i]->cur_id = device_id;
      in_threads_[i]->wakeup();
    }
  }

  // TODO:
  // need to handle the node graph more sensibly
  // should probably rebuild each threads node vector
  // when we add *or* delete a node
  void register_out_thread(SSS_Node<float> *node) {
    // auto thread = new SSS_Thread(cur_rr_index, node);
    // threads_[cur_rr_index]->set_node(node);
    threads_[cur_rr_index]->push_node(node);
    cur_rr_index += 1;
    if (cur_rr_index == n_out_threads)
      cur_rr_index = 0;
  }

  void register_in_thread(SSS_Node<float> *node) {
    // auto thread = new SSS_Thread(cur_rr_in_index, node);

    // in_threads_[cur_rr_in_index]->set_node(node);
    in_threads_[cur_rr_in_index]->push_node(node);
    cur_rr_in_index += 1;
    if (cur_rr_in_index == n_in_threads)
      cur_rr_in_index = 0;
  }

  // registers a node list to single thread
  void register_output_node_list(SSS_NodeList<float> *node_list) {}

  void register_out_thread_ecs(size_t idx) {
    threads_[cur_rr_index]->push_node_ecs(idx);
    if (cur_rr_index == n_out_threads)
      cur_rr_index = 0;
  }

  void register_in_thread_ecs(size_t idx) {
    threads_[cur_rr_in_index]->push_node_ecs(idx);
    if (cur_rr_in_index == n_in_threads)
      cur_rr_in_index = 0;
  }

  SSS_NodeECS<MAX_NODES> *node_ecs_;

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
  std::counting_semaphore<1> sem{0};
  std::size_t n_out_threads;
  std::size_t n_in_threads;
  std::size_t cur_rr_index{0};
  std::size_t cur_rr_in_index{0};

  bool stop;
};
