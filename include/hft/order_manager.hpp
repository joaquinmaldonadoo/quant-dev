#pragma once

#include <array>
#include <vector>

#include "hft/avellaneda_stoikov.hpp"
#include "hft/config.hpp"
#include "hft/messages.hpp"
#include "hft/risk_manager.hpp"

namespace hft {

enum class OrderState : std::uint8_t { Free, PendingNew, Live, PendingCancel };

struct OmsOrder {
  OrderId id = 0;
  Side side = Side::Bid;
  Price price = 0;
  Qty leaves = 0;
  OrderState state = OrderState::Free;
};

struct OmsStats {
  long news = 0;
  long cancels = 0;
  long rejects = 0;
  long fills = 0;
  long cancel_rejects = 0;
  long skipped_full = 0;
};

class OrderManager {
 public:
  static constexpr std::size_t kMaxOrders = 16;

  explicit OrderManager(const StrategyConfig& cfg) : cfg_(cfg) {}

  void on_quotes(const QuotePair& desired, Timestamp ts, RiskManager& risk, std::vector<OrderRequest>& out);
  void cancel_all(std::vector<OrderRequest>& out);
  void on_report(const ExecReport& r);

  Qty position() const { return position_; }
  Qty open_qty(Side side) const;
  const OmsOrder* working(Side side) const;
  int active_orders() const;
  const OmsStats& stats() const { return stats_; }

 private:
  OmsOrder* find(OrderId id);
  OmsOrder* free_slot();
  OmsOrder* working_mut(Side side);
  void manage_side(Side side, const Quote& want, Timestamp ts, RiskManager& risk, std::vector<OrderRequest>& out);

  StrategyConfig cfg_;
  std::array<OmsOrder, kMaxOrders> orders_{};
  std::array<Timestamp, 2> last_change_ts_{{-1'000'000'000'000, -1'000'000'000'000}};
  OrderId next_id_ = 1;
  Qty position_ = 0;
  OmsStats stats_;
};

}
