#pragma once

#include "hft/types.hpp"

namespace hft {

class Portfolio {
 public:
  explicit Portfolio(double maker_fee_bps = 0.0) : fee_rate_(maker_fee_bps / 10'000.0) {}

  void on_fill(Side side, Price price, Qty qty);

  Qty position() const { return position_; }
  double cash() const { return cash_; }
  double fees() const { return fees_; }
  double volume() const { return volume_; }
  long fills() const { return fills_; }
  Qty max_abs_position() const { return max_abs_position_; }

  double equity(double mid) const { return cash_ + static_cast<double>(position_) * mid; }

 private:
  double fee_rate_;
  Qty position_ = 0;
  Qty max_abs_position_ = 0;
  double cash_ = 0;
  double fees_ = 0;
  double volume_ = 0;
  long fills_ = 0;
};

}
