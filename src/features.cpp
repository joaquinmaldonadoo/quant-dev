#include "hft/features.hpp"

#include <algorithm>
#include <cmath>

namespace hft {
namespace {

constexpr double kNsPerSec = 1e9;

double decay(Timestamp dt_ns, double halflife_s) {
  return std::exp(-std::log(2.0) * (static_cast<double>(dt_ns) / kNsPerSec) / halflife_s);
}

}

double ofi_contribution(Price prev_bid, Qty prev_bid_qty, Price prev_ask, Qty prev_ask_qty,
                        Price bid, Qty bid_qty, Price ask, Qty ask_qty) {
  double e = 0;
  if (bid >= prev_bid) e += static_cast<double>(bid_qty);
  if (bid <= prev_bid) e -= static_cast<double>(prev_bid_qty);
  if (ask <= prev_ask) e -= static_cast<double>(ask_qty);
  if (ask >= prev_ask) e += static_cast<double>(prev_ask_qty);
  return e;
}

FeatureEngine::FeatureEngine(const StrategyConfig& cfg) : cfg_(cfg) {
  ring_.resize(1024);
  levels_buf_.resize(static_cast<std::size_t>(cfg.imbalance_levels));
  var_rate_ = cfg.vol_floor * cfg.vol_floor;
}

void FeatureEngine::push_ofi(Timestamp ts, double e) {
  if (ring_size_ == ring_.size()) {
    std::vector<OfiEntry> bigger(ring_.size() * 2);
    for (std::size_t i = 0; i < ring_size_; ++i) bigger[i] = ring_[(ring_head_ + i) % ring_.size()];
    ring_.swap(bigger);
    ring_head_ = 0;
  }
  ring_[(ring_head_ + ring_size_) % ring_.size()] = {ts, e};
  ++ring_size_;
  ofi_sum_ += e;
}

void FeatureEngine::evict_ofi(Timestamp now) {
  while (ring_size_ > 0 && ring_[ring_head_].ts <= now - cfg_.ofi_window_ns) {
    ofi_sum_ -= ring_[ring_head_].e;
    ring_head_ = (ring_head_ + 1) % ring_.size();
    --ring_size_;
  }
  if (ring_size_ == 0) ofi_sum_ = 0;
}

void FeatureEngine::update_vol(Timestamp ts, double mid) {
  if (!vol_started_) {
    vol_started_ = true;
    vol_last_ts_ = ts;
    vol_last_mid_ = mid;
    return;
  }
  const Timestamp dt = ts - vol_last_ts_;
  if (dt < kVolSampleNs) return;
  const double r = mid - vol_last_mid_;
  const double inst = r * r / (static_cast<double>(dt) / kNsPerSec);
  const double w = decay(dt, cfg_.vol_halflife_s);
  var_rate_ = w * var_rate_ + (1 - w) * inst;
  vol_last_ts_ = ts;
  vol_last_mid_ = mid;
}

void FeatureEngine::on_book(const OrderBook& book, Timestamp ts) {
  snap_.ts = ts;
  evict_ofi(ts);
  if (!book.has_both_sides() || book.crossed()) {
    snap_.valid = false;
    have_prev_ = false;
    return;
  }

  const Price bid = book.best_bid();
  const Price ask = book.best_ask();
  const Qty qb = book.best_qty(Side::Bid);
  const Qty qa = book.best_qty(Side::Ask);

  if (have_prev_) {
    const double e = ofi_contribution(prev_bid_, prev_bid_qty_, prev_ask_, prev_ask_qty_, bid, qb, ask, qa);
    if (e != 0) push_ofi(ts, e);
  }
  have_prev_ = true;
  prev_bid_ = bid;
  prev_ask_ = ask;
  prev_bid_qty_ = qb;
  prev_ask_qty_ = qa;

  const double depth = 0.5 * static_cast<double>(qb + qa);
  if (depth_ewma_ == 0) {
    depth_ewma_ = depth;
  } else {
    const double w = decay(ts - last_depth_ts_, cfg_.vol_halflife_s);
    depth_ewma_ = w * depth_ewma_ + (1 - w) * depth;
  }
  last_depth_ts_ = ts;

  const double mid = book.mid();
  snap_.mid = mid;
  snap_.spread = ask - bid;
  snap_.microprice = (static_cast<double>(bid) * static_cast<double>(qa) +
                      static_cast<double>(ask) * static_cast<double>(qb)) /
                     static_cast<double>(qa + qb);

  const int n = cfg_.imbalance_levels;
  double vb = 0, va = 0;
  const int nb = book.top_levels(Side::Bid, n, levels_buf_.data());
  for (int i = 0; i < nb; ++i) vb += static_cast<double>(levels_buf_[static_cast<std::size_t>(i)].qty);
  const int na = book.top_levels(Side::Ask, n, levels_buf_.data());
  for (int i = 0; i < na; ++i) va += static_cast<double>(levels_buf_[static_cast<std::size_t>(i)].qty);
  snap_.imbalance = (vb - va) / (vb + va);

  snap_.ofi = ofi_sum_;
  snap_.ofi_norm = depth_ewma_ > 0 ? ofi_sum_ / depth_ewma_ : 0;

  update_vol(ts, mid);
  snap_.sigma = std::max(std::sqrt(var_rate_), cfg_.vol_floor);

  const double raw = cfg_.beta_ofi * snap_.ofi_norm + cfg_.beta_micro * (snap_.microprice - mid);
  snap_.alpha = std::clamp(raw, -cfg_.signal_clip_ticks, cfg_.signal_clip_ticks);
  snap_.valid = true;
}

}
