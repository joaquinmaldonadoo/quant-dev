#pragma once

#include <vector>

#include "hft/messages.hpp"
#include "hft/order_book.hpp"
#include "hft/types.hpp"

namespace hft {

class ExchangeSim {
 public:
  ExchangeSim() { resting_.reserve(32); }

  void on_request(const OrderRequest& req, const OrderBook& book, Timestamp ts, std::vector<ExecReport>& out);
  void on_trade(const MarketEvent& trade, std::vector<ExecReport>& out);
  void on_book_update(Side side, Price price, Qty old_qty, Qty new_qty, const OrderBook& book, Timestamp ts,
                      std::vector<ExecReport>& out);

  std::size_t resting_count() const { return resting_.size(); }
  double queue_ahead(OrderId id) const;

 private:
  struct Resting {
    OrderId id;
    Side side;
    Price price;
    Qty leaves;
    double queue_ahead;
    Qty unreflected_trades;
  };

  void fill(Resting& o, Qty qty, Timestamp ts, std::vector<ExecReport>& out);
  void remove_done();

  std::vector<Resting> resting_;
};

}
