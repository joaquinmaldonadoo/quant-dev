#include "hft/exchange_sim.hpp"

#include <algorithm>

namespace hft {

void ExchangeSim::fill(Resting& o, Qty qty, Timestamp ts, std::vector<ExecReport>& out) {
  if (qty <= 0) return;
  o.leaves -= qty;
  out.push_back({ReportType::Fill, o.id, o.side, o.price, qty, o.leaves, ts});
}

void ExchangeSim::remove_done() {
  resting_.erase(std::remove_if(resting_.begin(), resting_.end(), [](const Resting& o) { return o.leaves <= 0; }),
                 resting_.end());
}

void ExchangeSim::on_request(const OrderRequest& req, const OrderBook& book, Timestamp ts,
                             std::vector<ExecReport>& out) {
  if (req.type == RequestType::Cancel) {
    const auto it = std::find_if(resting_.begin(), resting_.end(), [&](const Resting& o) { return o.id == req.id; });
    if (it == resting_.end()) {
      out.push_back({ReportType::CancelRejected, req.id, req.side, req.price, 0, 0, ts});
      return;
    }
    out.push_back({ReportType::Cancelled, req.id, it->side, it->price, 0, 0, ts});
    resting_.erase(it);
    return;
  }

  const bool crosses = req.side == Side::Bid ? (book.best_ask() != kNoPrice && req.price >= book.best_ask())
                                             : (book.best_bid() != kNoPrice && req.price <= book.best_bid());
  if (crosses || req.qty <= 0) {
    out.push_back({ReportType::Rejected, req.id, req.side, req.price, 0, 0, ts});
    return;
  }
  const auto ahead = static_cast<double>(book.qty_at(req.side, req.price));
  resting_.push_back({req.id, req.side, req.price, req.qty, ahead, 0});
  out.push_back({ReportType::Accepted, req.id, req.side, req.price, 0, req.qty, ts});
}

void ExchangeSim::on_trade(const MarketEvent& t, std::vector<ExecReport>& out) {
  const Side passive = opposite(t.side);
  for (auto& o : resting_) {
    if (o.side != passive || o.leaves <= 0) continue;
    const bool through = passive == Side::Ask ? t.price > o.price : t.price < o.price;
    if (through) {
      fill(o, o.leaves, t.ts, out);
      continue;
    }
    if (t.price != o.price) continue;
    o.unreflected_trades += t.qty;
    const double ahead = o.queue_ahead;
    const double for_us = static_cast<double>(t.qty) - ahead;
    o.queue_ahead = std::max(0.0, ahead - static_cast<double>(t.qty));
    if (for_us > 0) fill(o, std::min(o.leaves, static_cast<Qty>(for_us)), t.ts, out);
  }
  remove_done();
}

void ExchangeSim::on_book_update(Side side, Price price, Qty old_qty, Qty new_qty, const OrderBook& book,
                                 Timestamp ts, std::vector<ExecReport>& out) {
  if (new_qty < old_qty) {
    const Qty decrease = old_qty - new_qty;
    for (auto& o : resting_) {
      if (o.side != side || o.price != price) continue;
      const Qty from_trades = std::min(decrease, o.unreflected_trades);
      o.unreflected_trades -= from_trades;
      const Qty cancels = decrease - from_trades;
      const Qty base = old_qty - from_trades;
      if (cancels > 0 && base > 0) {
        const double frac_ahead = std::min(1.0, o.queue_ahead / static_cast<double>(base));
        o.queue_ahead = std::max(0.0, o.queue_ahead - static_cast<double>(cancels) * frac_ahead);
      }
    }
  }
  for (auto& o : resting_) {
    if (o.leaves <= 0) continue;
    const bool hit = o.side == Side::Bid ? (book.best_ask() != kNoPrice && book.best_ask() <= o.price)
                                         : (book.best_bid() != kNoPrice && book.best_bid() >= o.price);
    if (hit) fill(o, o.leaves, ts, out);
  }
  remove_done();
}

double ExchangeSim::queue_ahead(OrderId id) const {
  for (const auto& o : resting_) {
    if (o.id == id) return o.queue_ahead;
  }
  return -1;
}

}
