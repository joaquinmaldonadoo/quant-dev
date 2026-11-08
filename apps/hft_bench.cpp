
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "hft/backtest_engine.hpp"
#include "hft/csv_feed.hpp"
#include "hft/order_book.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double ns_since(Clock::time_point t0) {
  return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count());
}

}

int main(int argc, char** argv) {
  std::string config_path, data_path;
  int reps = 5;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string a = argv[i];
    if (a == "--config") config_path = argv[i + 1];
    else if (a == "--data") data_path = argv[i + 1];
    else if (a == "--reps") reps = std::stoi(argv[i + 1]);
  }
  if (config_path.empty() || data_path.empty() || reps < 1) {
    std::cerr << "uso: hft_bench --config ARCHIVO --data ARCHIVO [--reps N]\n";
    return 2;
  }

  try {
    const auto cfg = hft::load_config(config_path);
    const auto events = hft::load_events_csv(data_path);
    if (events.empty()) {
      std::cerr << "el archivo no tiene eventos\n";
      return 1;
    }
    const auto n = static_cast<double>(events.size());
    std::cout << "eventos: " << events.size() << ", repeticiones: " << reps << "\n";

    std::vector<double> book_ns;
    long checksum = 0;
    for (int r = 0; r < reps; ++r) {
      hft::OrderBook book(cfg.book.min_price, cfg.book.max_price);
      const auto t0 = Clock::now();
      for (const auto& e : events) {
        if (e.type == hft::EventType::Book) book.set_level(e.side, e.price, e.qty);
      }
      book_ns.push_back(ns_since(t0) / n);
      checksum += static_cast<long>(book.best_bid());
    }

    std::vector<double> engine_ns;
    for (int r = 0; r < reps; ++r) {
      hft::EngineOptions opts;
      opts.measure_latency = false;
      hft::BacktestEngine engine(cfg, opts);
      hft::VectorSource src(events);
      const auto t0 = Clock::now();
      const auto s = engine.run(src);
      engine_ns.push_back(ns_since(t0) / n);
      checksum += s.fills;
    }

    std::sort(book_ns.begin(), book_ns.end());
    std::sort(engine_ns.begin(), engine_ns.end());
    std::cout << "order book: mediana " << book_ns[book_ns.size() / 2] << " ns/evento\n";
    std::cout << "motor completo: mediana " << engine_ns[engine_ns.size() / 2] << " ns/evento ("
              << 1e3 / engine_ns[engine_ns.size() / 2] << " M eventos/s)\n";
    std::cout << "(checksum " << checksum << ")\n";
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
