#pragma once

#include <fstream>
#include <queue>
#include <string>
#include <vector>

#include "hft/avellaneda_stoikov.hpp"
#include "hft/config.hpp"
#include "hft/csv_feed.hpp"
#include "hft/exchange_sim.hpp"
#include "hft/features.hpp"
#include "hft/messages.hpp"
#include "hft/metrics.hpp"
#include "hft/order_book.hpp"
#include "hft/order_manager.hpp"
#include "hft/portfolio.hpp"
#include "hft/risk_manager.hpp"

namespace hft {

struct EngineOptions {
  std::string output_dir;
  std::string features_path;
  Timestamp features_every_ns = 100'000'000;
  bool measure_latency = true;
  bool trade = true;
};

class BacktestEngine {
 public:
  BacktestEngine(const AppConfig& cfg, EngineOptions opts = {});

  Summary run(EventSource& source);

  const OrderBook& book() const { return book_; }
  const Portfolio& portfolio() const { return portfolio_; }
  const OrderManager& oms() const { return oms_; }

 private:
  struct Internal {
    Timestamp ts;
    std::uint64_t seq;
    bool to_exchange;
    OrderRequest req;
    ExecReport report;
  };
  struct Later {
    bool operator()(const Internal& a, const Internal& b) const {
      return a.ts != b.ts ? a.ts > b.ts : a.seq > b.seq;
    }
  };

  void process_internal_until(Timestamp ts);
  void on_market(const MarketEvent& ev);
  void handle_exchange_reports(Timestamp ts);
  void decide(Timestamp ts);
  void send_requests(Timestamp ts);
  void dump_features(Timestamp ts);

  AppConfig cfg_;
  EngineOptions opts_;
  OrderBook book_;
  FeatureEngine features_;
  AvellanedaStoikov strategy_;
  RiskManager risk_;
  OrderManager oms_;
  ExchangeSim exchange_;
  Portfolio portfolio_;
  Metrics metrics_;
  LatencyHistogram latency_;

  std::priority_queue<Internal, std::vector<Internal>, Later> pending_;
  std::uint64_t seq_ = 0;
  std::vector<ExecReport> reports_;
  std::vector<OrderRequest> requests_;
  bool cancelled_on_halt_ = false;
  Summary summary_;

  std::ofstream features_out_;
  Timestamp next_feature_ts_ = 0;
};

}
