#pragma once

#include "hft/config.hpp"
#include "hft/features.hpp"
#include "hft/types.hpp"

namespace hft {

struct Quote {
  bool active = false;
  Price price = 0;
  Qty qty = 0;
};

struct QuotePair {
  Quote bid;
  Quote ask;
  double reservation = 0;
  double half_spread = 0;
};

class AvellanedaStoikov {
 public:
  explicit AvellanedaStoikov(const StrategyConfig& cfg) : cfg_(cfg) {}

  double reservation_price(double mid, double alpha, Qty inventory, double sigma) const;
  double half_spread(double sigma) const;

  QuotePair quote(const FeatureSnapshot& f, Qty inventory, Price best_bid, Price best_ask) const;

 private:
  StrategyConfig cfg_;
};

}
