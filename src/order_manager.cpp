#include "hft/order_manager.hpp"

namespace hft {

OmsOrder* OrderManager::find(OrderId id) {
  for (auto& o : orders_) {
    if (o.state != OrderState::Free && o.id == id) return &o;
  }
  return nullptr;
}

OmsOrder* OrderManager::free_slot() {
  for (auto& o : orders_) {
    if (o.state == OrderState::Free) return &o;
  }
  return nullptr;
}

OmsOrder* OrderManager::working_mut(Side side) {
  for (auto& o : orders_) {
    if (o.side == side && (o.state == OrderState::PendingNew || o.state == OrderState::Live)) return &o;
  }
  return nullptr;
}

const OmsOrder* OrderManager::working(Side side) const {
  for (const auto& o : orders_) {
    if (o.side == side && (o.state == OrderState::PendingNew || o.state == OrderState::Live)) return &o;
  }
  return nullptr;
}

Qty OrderManager::open_qty(Side side) const {
  Qty q = 0;
  for (const auto& o : orders_) {
    if (o.state != OrderState::Free && o.side == side) q += o.leaves;
  }
  return q;
}

int OrderManager::active_orders() const {
  int n = 0;
  for (const auto& o : orders_) n += o.state != OrderState::Free ? 1 : 0;
  return n;
}

void OrderManager::manage_side(Side side, const Quote& want, Timestamp ts, RiskManager& risk,
                               std::vector<OrderRequest>& out) {
  OmsOrder* cur = working_mut(side);
  const auto si = static_cast<std::size_t>(side);

  if (!want.active) {
    if (cur != nullptr && risk.try_consume_messages(ts)) {
      cur->state = OrderState::PendingCancel;
      out.push_back({RequestType::Cancel, cur->id, side, cur->price, 0});
      ++stats_.cancels;
      last_change_ts_[si] = ts;
    }
    return;
  }

  if (cur != nullptr && cur->price == want.price) return;
  if (ts - last_change_ts_[si] < cfg_.min_requote_ns) return;

  OmsOrder* slot = free_slot();
  if (slot == nullptr) {
    ++stats_.skipped_full;
    return;
  }
  if (!risk.try_consume_messages(ts, cur != nullptr ? 2 : 1)) return;

  if (cur != nullptr) {
    cur->state = OrderState::PendingCancel;
    out.push_back({RequestType::Cancel, cur->id, side, cur->price, 0});
    ++stats_.cancels;
  }
  *slot = {next_id_++, side, want.price, want.qty, OrderState::PendingNew};
  out.push_back({RequestType::New, slot->id, side, want.price, want.qty});
  ++stats_.news;
  last_change_ts_[si] = ts;
}

void OrderManager::on_quotes(const QuotePair& desired, Timestamp ts, RiskManager& risk,
                             std::vector<OrderRequest>& out) {
  manage_side(Side::Bid, desired.bid, ts, risk, out);
  manage_side(Side::Ask, desired.ask, ts, risk, out);
}

void OrderManager::cancel_all(std::vector<OrderRequest>& out) {
  for (auto& o : orders_) {
    if (o.state == OrderState::PendingNew || o.state == OrderState::Live) {
      o.state = OrderState::PendingCancel;
      out.push_back({RequestType::Cancel, o.id, o.side, o.price, 0});
      ++stats_.cancels;
    }
  }
}

void OrderManager::on_report(const ExecReport& r) {
  OmsOrder* o = find(r.id);
  switch (r.type) {
    case ReportType::Accepted:
      if (o != nullptr && o->state == OrderState::PendingNew) o->state = OrderState::Live;
      break;
    case ReportType::Rejected:
      ++stats_.rejects;
      if (o != nullptr) o->state = OrderState::Free;
      break;
    case ReportType::Fill:
      ++stats_.fills;
      position_ += sign(r.side) * r.qty;
      if (o != nullptr) {
        o->leaves = r.leaves;
        if (o->leaves == 0) o->state = OrderState::Free;
      }
      break;
    case ReportType::Cancelled:
      if (o != nullptr) o->state = OrderState::Free;
      break;
    case ReportType::CancelRejected:
      ++stats_.cancel_rejects;
      break;
  }
}

}
