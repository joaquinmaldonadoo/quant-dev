#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <fstream>
#include <string>

#include "hft/types.hpp"

namespace hft {

class LatencyHistogram {
 public:
  static constexpr int kBucketNs = 10;
  static constexpr int kBuckets = 10'000;

  void add(std::int64_t ns);
  std::int64_t count() const { return count_; }
  std::int64_t percentile(double p) const;
  double mean() const { return count_ > 0 ? static_cast<double>(sum_) / static_cast<double>(count_) : 0; }
  std::int64_t max() const { return max_; }

 private:
  std::array<std::int64_t, kBuckets> buckets_{};
  std::int64_t count_ = 0;
  std::int64_t sum_ = 0;
  std::int64_t max_ = 0;
};

struct Summary {
  long market_events = 0;
  long trades_seen = 0;
  double duration_s = 0;

  double final_equity = 0;
  double final_pnl_currency = 0;
  double fees = 0;
  double volume = 0;
  long fills = 0;
  Qty final_position = 0;
  Qty max_abs_position = 0;
  double avg_abs_position = 0;

  double max_drawdown = 0;
  long samples = 0;
  double pnl_mean_per_interval = 0;
  double pnl_std_per_interval = 0;
  double sharpe_per_interval = 0;
  double sharpe_annualized = 0;

  long markouts = 0;
  double avg_markout = 0;
  double adverse_fill_ratio = 0;

  long orders_sent = 0;
  long cancels_sent = 0;
  long rejects = 0;
  long cancel_rejects = 0;
  long throttled = 0;
  bool halted = false;

  std::int64_t latency_p50_ns = 0;
  std::int64_t latency_p99_ns = 0;
  std::int64_t latency_p999_ns = 0;
  std::int64_t latency_max_ns = 0;
  double latency_mean_ns = 0;
  bool latency_measured = false;
};

std::string summary_to_json(const Summary& s);

class Metrics {
 public:
  Metrics(Timestamp sample_interval_ns, Timestamp markout_ns, double currency_per_unit);

  void open_outputs(const std::string& dir);

  void on_state(Timestamp ts, double mid, Qty position, double equity, double fees);
  void on_fill(Timestamp ts, Side side, Price price, Qty qty, double mid);

  void finish(Summary& s);

 private:
  void write_sample(Timestamp ts, double mid, Qty position, double equity, double fees);
  void resolve_markouts(Timestamp ts, double mid);

  struct PendingFill {
    Timestamp ts;
    Side side;
    Price price;
    Qty qty;
    double mid_at_fill;
  };

  Timestamp interval_;
  Timestamp markout_ns_;
  double currency_per_unit_;

  bool started_ = false;
  Timestamp first_ts_ = 0;
  Timestamp last_ts_ = 0;
  Timestamp next_sample_ = 0;
  double last_equity_ = 0;
  Qty last_position_ = 0;
  double last_sample_equity_ = 0;
  double peak_equity_ = 0;
  double max_dd_ = 0;
  double abs_pos_time_ = 0;

  long samples_ = 0;
  double sum_d_ = 0, sum_d2_ = 0;

  std::deque<PendingFill> pending_;
  long markouts_ = 0;
  double markout_sum_ = 0;
  long adverse_ = 0;

  std::ofstream pnl_out_;
  std::ofstream fills_out_;
};

}
