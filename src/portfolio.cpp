#include "hft/portfolio.hpp"

#include <cstdlib>

namespace hft {

void Portfolio::on_fill(Side side, Price price, Qty qty) {
  const double notional = static_cast<double>(price) * static_cast<double>(qty);
  const double fee = notional * fee_rate_;
  cash_ -= sign(side) * notional;
  cash_ -= fee;
  fees_ += fee;
  volume_ += notional;
  position_ += sign(side) * qty;
  if (std::llabs(position_) > max_abs_position_) max_abs_position_ = std::llabs(position_);
  ++fills_;
}

}
