#include <atomic>
#include <cstdlib>

template <typename T> class SSS_Fifo {
public:
  std::size_t size;
  std::atomic<std::size_t> head;
  std::atomic<std::size_t> tail;
  std::atomic<std::size_t> cur_capacity;

  SSS_Fifo(std::size_t queue_size) : size(queue_size) {
    data = new T[size];
    head = 0;
    tail = 0;
    cur_capacity = queue_size;
  }

  bool is_full() {
    auto cur_head = head.load(std::memory_order_relaxed);
    auto cur_tail = tail.load(std::memory_order_relaxed);
    if (cur_head == cur_tail && (!(cur_head == 0 && cur_tail == 0)))
      return true;
    else
      return false;
  }

  std::size_t get_capacity() {
    return cur_capacity.load(std::memory_order_relaxed);
  }

  bool enqueue(T x) {
    auto cur_tail = tail.load(std::memory_order_relaxed);
    auto new_tail_idx = tail + 1;
    if (new_tail_idx == size)
      new_tail_idx = 0;
    if (new_tail_idx == head.load(std::memory_order_acquire))
      return false;
    data[cur_tail] = x;
    tail.store(new_tail_idx, std::memory_order_release);
    auto cur_cap = cur_capacity.load(std::memory_order_relaxed) - 1;
    cur_capacity.store(cur_cap, std::memory_order_relaxed);
    return true;
  }

  bool dequeue(T &in) {
    auto cur_head = head.load(std::memory_order_relaxed);
    if (cur_head == tail.load(std::memory_order_acquire))
      return false;
    in = data[cur_head];
    auto new_head_idx = cur_head + 1;
    if (new_head_idx == size)
      new_head_idx = 0;
    head.store(new_head_idx, std::memory_order_release);
    auto cur_cap = cur_capacity.load(std::memory_order_relaxed) + 1;
    cur_capacity.store(cur_cap, std::memory_order_relaxed);
    return true;
  }

  bool dequeue_n(T *in, std::size_t n) {
    auto cur_head = head.load(std::memory_order_relaxed);
    if (cur_head == tail.load(std::memory_order_relaxed))
      return false;
    in = data[cur_head];
    auto new_head_idx = cur_head + 1;
    if (new_head_idx == size)
      new_head_idx = 0;
    head.store(new_head_idx, std::memory_order_relaxed);
    return true;
  }

private:
  T *data;
};
