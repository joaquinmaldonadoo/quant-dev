#pragma once

#include <string>
#include <vector>

#include "hft/avellaneda_stoikov.hpp"
#include "hft/config.hpp"

namespace hft {

class RiskManager {
 public:
  explicit RiskManager(const RiskConfig& cfg);

  void filter(QuotePair& q, Qty inventory, Qty open_bid_qty, Qty open_ask_qty) const;

  bool try_consume_messages(Timestamp ts, int n = 1);

  void on_equity(double equity);

  bool halted() const { return halted_; }
  const std::string& halt_reason() const { return halt_reason_; }
  long throttled() const { return throttled_; }

 private:
  RiskConfig cfg_;
  std::vector<Timestamp> msg_times_;
  std::size_t msg_pos_ = 0;
  std::size_t msg_count_ = 0;
  double peak_equity_ = 0;
  bool have_peak_ = false;
  bool halted_ = false;
  std::string halt_reason_;
  long throttled_ = 0;
};

}
