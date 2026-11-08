#include "hft/metrics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace hft {

void LatencyHistogram::add(std::int64_t ns) {
  if (ns < 0) ns = 0;
  const auto b = std::min<std::int64_t>(ns / kBucketNs, kBuckets - 1);
  ++buckets_[static_cast<std::size_t>(b)];
  ++count_;
  sum_ += ns;
  max_ = std::max(max_, ns);
}

std::int64_t LatencyHistogram::percentile(double p) const {
  if (count_ == 0) return 0;
  const auto target = static_cast<std::int64_t>(std::ceil(p / 100.0 * static_cast<double>(count_)));
  std::int64_t acc = 0;
  for (int i = 0; i < kBuckets; ++i) {
    acc += buckets_[static_cast<std::size_t>(i)];
    if (acc >= std::max<std::int64_t>(target, 1)) return static_cast<std::int64_t>(i + 1) * kBucketNs;
  }
  return static_cast<std::int64_t>(kBuckets) * kBucketNs;
}

Metrics::Metrics(Timestamp sample_interval_ns, Timestamp markout_ns, double currency_per_unit)
    : interval_(sample_interval_ns), markout_ns_(markout_ns), currency_per_unit_(currency_per_unit) {}

void Metrics::open_outputs(const std::string& dir) {
  std::filesystem::create_directories(dir);
  pnl_out_.open(dir + "/pnl.csv");
  fills_out_.open(dir + "/fills.csv");
  if (!pnl_out_ || !fills_out_) throw std::runtime_error("no se pudieron crear los CSV en " + dir);
  pnl_out_ << "ts_ns,mid,position,equity,fees\n";
  fills_out_ << "ts_ns,side,price,qty,mid_at_fill,mid_after,markout\n";
}

void Metrics::write_sample(Timestamp ts, double mid, Qty position, double equity, double fees) {
  if (samples_ > 0) {
    const double d = equity - last_sample_equity_;
    sum_d_ += d;
    sum_d2_ += d * d;
  }
  ++samples_;
  last_sample_equity_ = equity;
  if (pnl_out_.is_open()) pnl_out_ << ts << ',' << mid << ',' << position << ',' << equity << ',' << fees << '\n';
}

void Metrics::resolve_markouts(Timestamp ts, double mid) {
  while (!pending_.empty() && pending_.front().ts + markout_ns_ <= ts) {
    const auto& f = pending_.front();
    const double m = (mid - static_cast<double>(f.price)) * sign(f.side);
    markout_sum_ += m * static_cast<double>(f.qty);
    markouts_ += f.qty;
    if (m < 0) adverse_ += f.qty;
    if (fills_out_.is_open()) {
      fills_out_ << f.ts << ',' << (f.side == Side::Bid ? 'B' : 'A') << ',' << f.price << ',' << f.qty << ','
                 << f.mid_at_fill << ',' << mid << ',' << m << '\n';
    }
    pending_.pop_front();
  }
}

void Metrics::on_state(Timestamp ts, double mid, Qty position, double equity, double fees) {
  if (!started_) {
    started_ = true;
    first_ts_ = ts;
    next_sample_ = ts;
    peak_equity_ = equity;
  } else {
    abs_pos_time_ += static_cast<double>(std::llabs(last_position_)) * static_cast<double>(ts - last_ts_);
  }
  last_ts_ = ts;
  last_position_ = position;
  last_equity_ = equity;

  peak_equity_ = std::max(peak_equity_, equity);
  max_dd_ = std::max(max_dd_, peak_equity_ - equity);

  resolve_markouts(ts, mid);

  if (ts >= next_sample_) {
    write_sample(ts, mid, position, equity, fees);
    next_sample_ = first_ts_ + ((ts - first_ts_) / interval_ + 1) * interval_;
  }
}

void Metrics::on_fill(Timestamp ts, Side side, Price price, Qty qty, double mid) {
  pending_.push_back({ts, side, price, qty, mid});
}

void Metrics::finish(Summary& s) {
  for (const auto& f : pending_) {
    if (fills_out_.is_open()) {
      fills_out_ << f.ts << ',' << (f.side == Side::Bid ? 'B' : 'A') << ',' << f.price << ',' << f.qty << ','
                 << f.mid_at_fill << ",,\n";
    }
  }
  pending_.clear();

  const double dur_ns = static_cast<double>(last_ts_ - first_ts_);
  s.duration_s = dur_ns / 1e9;
  s.final_equity = last_equity_;
  s.final_pnl_currency = last_equity_ * currency_per_unit_;
  s.max_drawdown = max_dd_;
  s.avg_abs_position = dur_ns > 0 ? abs_pos_time_ / dur_ns : 0;
  s.samples = samples_;
  const long n = samples_ - 1;
  if (n >= 2) {
    const double mean = sum_d_ / static_cast<double>(n);
    const double var = (sum_d2_ - static_cast<double>(n) * mean * mean) / static_cast<double>(n - 1);
    s.pnl_mean_per_interval = mean;
    s.pnl_std_per_interval = var > 0 ? std::sqrt(var) : 0;
    if (s.pnl_std_per_interval > 0) {
      s.sharpe_per_interval = mean / s.pnl_std_per_interval;
      const double per_year = 365.0 * 24 * 3600 * 1e9 / static_cast<double>(interval_);
      s.sharpe_annualized = s.sharpe_per_interval * std::sqrt(per_year);
    }
  }
  s.markouts = markouts_;
  s.avg_markout = markouts_ > 0 ? markout_sum_ / static_cast<double>(markouts_) : 0;
  s.adverse_fill_ratio = markouts_ > 0 ? static_cast<double>(adverse_) / static_cast<double>(markouts_) : 0;
  if (pnl_out_.is_open()) pnl_out_.flush();
  if (fills_out_.is_open()) fills_out_.flush();
}

std::string summary_to_json(const Summary& s) {
  std::ostringstream o;
  o.precision(10);
  o << "{\n";
  auto kv = [&](const char* k, auto v, bool last = false) {
    o << "  \"" << k << "\": " << v << (last ? "\n" : ",\n");
  };
  kv("market_events", s.market_events);
  kv("trades_seen", s.trades_seen);
  kv("duration_s", s.duration_s);
  kv("final_equity_ticks", s.final_equity);
  kv("final_pnl_currency", s.final_pnl_currency);
  kv("fees_ticks", s.fees);
  kv("volume_ticks", s.volume);
  kv("fills", s.fills);
  kv("final_position", s.final_position);
  kv("max_abs_position", s.max_abs_position);
  kv("avg_abs_position", s.avg_abs_position);
  kv("max_drawdown_ticks", s.max_drawdown);
  kv("samples", s.samples);
  kv("pnl_mean_per_interval", s.pnl_mean_per_interval);
  kv("pnl_std_per_interval", s.pnl_std_per_interval);
  kv("sharpe_per_interval", s.sharpe_per_interval);
  kv("sharpe_annualized", s.sharpe_annualized);
  kv("markout_lots", s.markouts);
  kv("avg_markout_ticks", s.avg_markout);
  kv("adverse_fill_ratio", s.adverse_fill_ratio);
  kv("orders_sent", s.orders_sent);
  kv("cancels_sent", s.cancels_sent);
  kv("rejects", s.rejects);
  kv("cancel_rejects", s.cancel_rejects);
  kv("throttled", s.throttled);
  kv("halted", s.halted ? "true" : "false");
  kv("latency_measured", s.latency_measured ? "true" : "false");
  kv("latency_mean_ns", s.latency_mean_ns);
  kv("latency_p50_ns", s.latency_p50_ns);
  kv("latency_p99_ns", s.latency_p99_ns);
  kv("latency_p999_ns", s.latency_p999_ns);
  kv("latency_max_ns", s.latency_max_ns, true);
  o << "}\n";
  return o.str();
}

}
