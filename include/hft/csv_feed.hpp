#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "hft/spsc_queue.hpp"
#include "hft/types.hpp"

namespace hft {

class EventSource {
 public:
  virtual ~EventSource() = default;
  virtual bool next(MarketEvent& out) = 0;
};

class VectorSource : public EventSource {
 public:
  explicit VectorSource(std::vector<MarketEvent> events) : events_(std::move(events)) {}
  bool next(MarketEvent& out) override {
    if (pos_ >= events_.size()) return false;
    out = events_[pos_++];
    return true;
  }

 private:
  std::vector<MarketEvent> events_;
  std::size_t pos_ = 0;
};

MarketEvent parse_event_line(std::string_view line);

std::vector<MarketEvent> load_events_csv(const std::string& path);

class CsvFeed : public EventSource {
 public:
  explicit CsvFeed(const std::string& path, std::size_t queue_capacity = 1 << 16);
  ~CsvFeed() override;
  bool next(MarketEvent& out) override;

 private:
  void reader_loop();

  std::string path_;
  SpscQueue<MarketEvent> queue_;
  std::atomic<bool> done_{false};
  std::atomic<bool> stop_{false};
  std::string error_;
  std::thread reader_;
};

}
