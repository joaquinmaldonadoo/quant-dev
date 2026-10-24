#include "hft/avellaneda_stoikov.hpp"

#include <algorithm>
#include <cmath>

namespace hft {

double AvellanedaStoikov::reservation_price(double mid, double alpha, Qty inventory, double sigma) const {
  return mid + alpha - static_cast<double>(inventory) * cfg_.gamma * sigma * sigma * cfg_.tau_s;
}

double AvellanedaStoikov::half_spread(double sigma) const {
  return 0.5 * cfg_.gamma * sigma * sigma * cfg_.tau_s + std::log(1.0 + cfg_.gamma / cfg_.k) / cfg_.gamma;
}

QuotePair AvellanedaStoikov::quote(const FeatureSnapshot& f, Qty inventory, Price best_bid, Price best_ask) const {
  QuotePair q;
  if (!f.valid) return q;
  q.reservation = reservation_price(f.mid, f.alpha, inventory, f.sigma);
  q.half_spread = half_spread(f.sigma);

  auto bid = static_cast<Price>(std::floor(q.reservation - q.half_spread));
  auto ask = static_cast<Price>(std::ceil(q.reservation + q.half_spread));
  bid = std::min(bid, best_ask - 1);
  ask = std::max(ask, best_bid + 1);

  q.bid = {true, bid, cfg_.order_qty};
  q.ask = {true, ask, cfg_.order_qty};
  return q;
}

}
