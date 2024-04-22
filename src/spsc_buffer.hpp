#include <atomic>
#include <cassert>
// #include <stdexcept>

template <typename T> class SPSC_Queue {
public:
  SPSC_Queue(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ < 1) {
      capacity_ = 1;
    }
    capacity_++;

    if (capacity_ > SIZE_MAX - 2 * kPadding) {
      capacity_ = SIZE_MAX - 2 * kPadding;
    }
  }

  size_t capacity() const noexcept { return capacity_ - 1; }

  bool empty() const noexcept {
    return writeIdx_.load(std::memory_order_acquire) ==
           readIdx_.load(std::memory_order_acquire);
  }

  void push(T data) noexcept {
    auto const writeIdx = writeIdx_.load(std::memory_order_relaxed);
    auto nextWriteIdx = writeIdx + 1;
    if (nextWriteIdx == capacity_) {
      nextWriteIdx = 0;
    }
    while (nextWriteIdx == readIdxCache_) {
      readIdxCache_ = readIdx_.load(std::memory_order_acquire);
    }
    slots_[writeIdx + kPadding] = data;
    writeIdx_.store(nextWriteIdx, std::memory_order_release);
  }

  T pop() noexcept {
    auto const readIdx = readIdx_.load(std::memory_order_relaxed);
    assert(writeIdx_.load(std::memory_order_acquire) != readIdx &&
           "Can only call pop() after front() has returned a non-nullptr");
    auto res = slots_[readIdx + kPadding];
    auto nextReadIdx = readIdx + 1;
    if (nextReadIdx == capacity_) {
      nextReadIdx = 0;
    }
    readIdx_.store(nextReadIdx, std::memory_order_release);
    return res;
  }

  void write_n(T *src, size_t n_samples) noexcept {
    for (size_t i = 0; i < n_samples; i++) {
      push(src[i]);
    }
  }

  T *read_n(T *dest, size_t n_samples) noexcept {
    for (size_t i = 0; i < n_samples; i++) {
      dest[i] = pop();
    }
    return dest;
  };

private:
  std::size_t capacity_;
  static constexpr size_t kCacheLineSize = 64;
  static constexpr size_t kPadding = (kCacheLineSize - 1) / sizeof(T) + 1;
  T *slots_;
  alignas(kCacheLineSize) std::atomic<size_t> writeIdx_ = {0};
  alignas(kCacheLineSize) size_t writeIdxCache_ = {0};
  alignas(kCacheLineSize) std::atomic<size_t> readIdx_ = {0};
  alignas(kCacheLineSize) size_t readIdxCache_ = {0};
};
