#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <vector>

namespace hft {

template <typename T>
class SpscQueue {
 public:
  explicit SpscQueue(std::size_t capacity) : buffer_(capacity), mask_(capacity - 1) {
    if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
      throw std::invalid_argument("la capacidad de la cola tiene que ser potencia de 2");
    }
  }

  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;

  bool try_push(const T& value) {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail - head_cache_ == buffer_.size()) {
      head_cache_ = head_.load(std::memory_order_acquire);
      if (tail - head_cache_ == buffer_.size()) return false;
    }
    buffer_[tail & mask_] = value;
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  bool try_pop(T& out) {
    const auto head = head_.load(std::memory_order_relaxed);
    if (head == tail_cache_) {
      tail_cache_ = tail_.load(std::memory_order_acquire);
      if (head == tail_cache_) return false;
    }
    out = buffer_[head & mask_];
    head_.store(head + 1, std::memory_order_release);
    return true;
  }

  std::size_t capacity() const { return buffer_.size(); }

 private:
  static constexpr std::size_t kCacheLine = 64;

  std::vector<T> buffer_;
  const std::size_t mask_;
  alignas(kCacheLine) std::atomic<std::size_t> head_{0};
  std::size_t tail_cache_ = 0;
  alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
  std::size_t head_cache_ = 0;
};

}
