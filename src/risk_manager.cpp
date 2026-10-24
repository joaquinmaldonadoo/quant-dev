#include "hft/risk_manager.hpp"

#include <algorithm>

namespace hft {

RiskManager::RiskManager(const RiskConfig& cfg)
    : cfg_(cfg), msg_times_(static_cast<std::size_t>(cfg.max_orders_per_sec), 0) {}

void RiskManager::filter(QuotePair& q, Qty inventory, Qty open_bid_qty, Qty open_ask_qty) const {
  if (halted_) {
    q.bid.active = false;
    q.ask.active = false;
    return;
  }
  q.bid.qty = std::min(q.bid.qty, cfg_.max_order_qty);
  q.ask.qty = std::min(q.ask.qty, cfg_.max_order_qty);
  if (inventory + open_bid_qty + q.bid.qty > cfg_.max_inventory) q.bid.active = false;
  if (inventory - open_ask_qty - q.ask.qty < -cfg_.max_inventory) q.ask.active = false;
}

bool RiskManager::try_consume_messages(Timestamp ts, int n) {
  const std::size_t size = msg_times_.size();
  const auto need = static_cast<std::size_t>(n);
  if (need > size) {
    ++throttled_;
    return false;
  }
  const std::size_t free = size - msg_count_;
  if (need > free) {
    const std::size_t oldest = (msg_pos_ + size - msg_count_) % size;
    const std::size_t idx = (oldest + (need - free) - 1) % size;
    if (ts - msg_times_[idx] < 1'000'000'000) {
      ++throttled_;
      return false;
    }
  }
  for (std::size_t i = 0; i < need; ++i) {
    msg_times_[msg_pos_] = ts;
    msg_pos_ = (msg_pos_ + 1) % size;
  }
  msg_count_ = std::min(msg_count_ + need, size);
  return true;
}

void RiskManager::on_equity(double equity) {
  if (!have_peak_ || equity > peak_equity_) {
    peak_equity_ = equity;
    have_peak_ = true;
  }
  if (!halted_ && peak_equity_ - equity > cfg_.max_drawdown) {
    halted_ = true;
    halt_reason_ = "drawdown máximo superado";
  }
}

}
