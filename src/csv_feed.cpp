#include "hft/csv_feed.hpp"

#include <charconv>
#include <fstream>
#include <stdexcept>

namespace hft {
namespace {

std::int64_t parse_int(std::string_view s, const char* field) {
  std::int64_t v = 0;
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec != std::errc() || ptr != s.data() + s.size()) {
    throw std::runtime_error(std::string("campo ") + field + " inválido: '" + std::string(s) + "'");
  }
  return v;
}

bool is_header(std::string_view line) { return line.rfind("ts", 0) == 0; }

class LineReader {
 public:
  explicit LineReader(const std::string& path) : in_(path) {
    if (!in_) throw std::runtime_error("no se pudo abrir " + path);
  }
  bool next(MarketEvent& ev) {
    while (std::getline(in_, line_)) {
      ++line_no_;
      if (!line_.empty() && line_.back() == '\r') line_.pop_back();
      if (line_.empty() || (line_no_ == 1 && is_header(line_))) continue;
      try {
        ev = parse_event_line(line_);
      } catch (const std::exception& e) {
        throw std::runtime_error("línea " + std::to_string(line_no_) + ": " + e.what());
      }
      if (ev.ts < last_ts_) {
        throw std::runtime_error("línea " + std::to_string(line_no_) + ": timestamp menor al anterior");
      }
      last_ts_ = ev.ts;
      return true;
    }
    return false;
  }

 private:
  std::ifstream in_;
  std::string line_;
  std::size_t line_no_ = 0;
  Timestamp last_ts_ = 0;
};

}

MarketEvent parse_event_line(std::string_view line) {
  std::string_view fields[5];
  std::size_t start = 0;
  for (int i = 0; i < 5; ++i) {
    const auto comma = line.find(',', start);
    if (i < 4 && comma == std::string_view::npos) throw std::runtime_error("faltan columnas");
    if (i == 4 && comma != std::string_view::npos) throw std::runtime_error("sobran columnas");
    fields[i] = line.substr(start, i < 4 ? comma - start : std::string_view::npos);
    start = comma + 1;
  }
  MarketEvent ev;
  ev.ts = parse_int(fields[0], "ts_ns");
  if (fields[1] == "B") {
    ev.type = EventType::Book;
  } else if (fields[1] == "T") {
    ev.type = EventType::Trade;
  } else {
    throw std::runtime_error("type tiene que ser B o T");
  }
  if (fields[2] == "B") {
    ev.side = Side::Bid;
  } else if (fields[2] == "A") {
    ev.side = Side::Ask;
  } else {
    throw std::runtime_error("side tiene que ser B o A");
  }
  ev.price = parse_int(fields[3], "price");
  ev.qty = parse_int(fields[4], "qty");
  if (ev.qty < 0) throw std::runtime_error("qty negativa");
  if (ev.type == EventType::Trade && ev.qty == 0) throw std::runtime_error("trade con qty 0");
  return ev;
}

std::vector<MarketEvent> load_events_csv(const std::string& path) {
  LineReader reader(path);
  std::vector<MarketEvent> out;
  MarketEvent ev;
  while (reader.next(ev)) out.push_back(ev);
  return out;
}

CsvFeed::CsvFeed(const std::string& path, std::size_t queue_capacity)
    : path_(path), queue_(queue_capacity) {
  std::ifstream probe(path);
  if (!probe) throw std::runtime_error("no se pudo abrir " + path);
  reader_ = std::thread([this] { reader_loop(); });
}

CsvFeed::~CsvFeed() {
  stop_.store(true, std::memory_order_relaxed);
  if (reader_.joinable()) reader_.join();
}

void CsvFeed::reader_loop() {
  try {
    LineReader reader(path_);
    MarketEvent ev;
    while (!stop_.load(std::memory_order_relaxed) && reader.next(ev)) {
      while (!queue_.try_push(ev)) {
        if (stop_.load(std::memory_order_relaxed)) return;
        std::this_thread::yield();
      }
    }
  } catch (const std::exception& e) {
    error_ = e.what();
  }
  done_.store(true, std::memory_order_release);
}

bool CsvFeed::next(MarketEvent& out) {
  for (;;) {
    if (queue_.try_pop(out)) return true;
    if (done_.load(std::memory_order_acquire)) {
      if (queue_.try_pop(out)) return true;
      if (!error_.empty()) throw std::runtime_error(path_ + ": " + error_);
      return false;
    }
    std::this_thread::yield();
  }
}

}
