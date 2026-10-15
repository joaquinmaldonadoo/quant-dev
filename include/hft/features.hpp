#pragma once

#include <vector>

#include "hft/config.hpp"
#include "hft/order_book.hpp"
#include "hft/types.hpp"

namespace hft {

struct FeatureSnapshot {
  Timestamp ts = 0;
  double mid = 0;
  double microprice = 0;
  double imbalance = 0;
  double ofi = 0;
  double ofi_norm = 0;
  double sigma = 0;
  double alpha = 0;
  Price spread = 0;
  bool valid = false;
};

class FeatureEngine {
 public:
  explicit FeatureEngine(const StrategyConfig& cfg);

  void on_book(const OrderBook& book, Timestamp ts);
  const FeatureSnapshot& snapshot() const { return snap_; }

  static constexpr Timestamp kVolSampleNs = 100'000'000;

 private:
  struct OfiEntry {
    Timestamp ts;
    double e;
  };
  void push_ofi(Timestamp ts, double e);
  void evict_ofi(Timestamp now);
  void update_vol(Timestamp ts, double mid);

  StrategyConfig cfg_;
  FeatureSnapshot snap_;

  bool have_prev_ = false;
  Price prev_bid_ = 0, prev_ask_ = 0;
  Qty prev_bid_qty_ = 0, prev_ask_qty_ = 0;

  std::vector<OfiEntry> ring_;
  std::size_t ring_head_ = 0;
  std::size_t ring_size_ = 0;
  double ofi_sum_ = 0;

  double depth_ewma_ = 0;
  Timestamp last_depth_ts_ = 0;

  bool vol_started_ = false;
  Timestamp vol_last_ts_ = 0;
  double vol_last_mid_ = 0;
  double var_rate_ = 0;

  std::vector<Level> levels_buf_;
};

double ofi_contribution(Price prev_bid, Qty prev_bid_qty, Price prev_ask, Qty prev_ask_qty,
                        Price bid, Qty bid_qty, Price ask, Qty ask_qty);

}
