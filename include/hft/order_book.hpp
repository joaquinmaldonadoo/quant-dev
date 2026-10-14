#pragma once

#include <vector>

#include "hft/types.hpp"

namespace hft {

struct Level {
  Price price = kNoPrice;
  Qty qty = 0;
};

class OrderBook {
 public:
  OrderBook(Price min_price, Price max_price);

  Qty set_level(Side side, Price price, Qty qty);

  Qty qty_at(Side side, Price price) const;
  Price best_bid() const { return best_bid_; }
  Price best_ask() const { return best_ask_; }
  Qty best_qty(Side side) const;

  bool has_both_sides() const { return best_bid_ != kNoPrice && best_ask_ != kNoPrice; }
  bool crossed() const { return has_both_sides() && best_bid_ >= best_ask_; }
  double mid() const { return 0.5 * static_cast<double>(best_bid_ + best_ask_); }
  Price spread() const { return best_ask_ - best_bid_; }

  int top_levels(Side side, int n, Level* out, Price max_scan_ticks = 1000) const;

  Price min_price() const { return min_price_; }
  Price max_price() const { return max_price_; }

  void clear();

 private:
  std::size_t index(Price p) const { return static_cast<std::size_t>(p - min_price_); }
  void check_range(Price p) const;
  Price scan_bid_from(Price p) const;
  Price scan_ask_from(Price p) const;

  Price min_price_;
  Price max_price_;
  std::vector<Qty> bids_;
  std::vector<Qty> asks_;
  Price best_bid_ = kNoPrice;
  Price best_ask_ = kNoPrice;
};

}
