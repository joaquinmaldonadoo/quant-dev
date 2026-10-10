#pragma once

#include <map>
#include <stdexcept>
#include <string>

#include "hft/types.hpp"

namespace hft {

struct ConfigError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct StrategyConfig {
  double gamma = 0.1;
  double k = 1.5;
  double tau_s = 10.0;
  Qty order_qty = 1;
  double vol_halflife_s = 30.0;
  double vol_floor = 0.05;
  double beta_ofi = 0.0;
  double beta_micro = 0.0;
  Timestamp ofi_window_ns = 1'000'000'000;
  int imbalance_levels = 3;
  double signal_clip_ticks = 2.0;
  Timestamp min_requote_ns = 1'000'000;
};

struct RiskConfig {
  Qty max_inventory = 10;
  Qty max_order_qty = 5;
  double max_drawdown = 1e18;
  int max_orders_per_sec = 200;
};

struct SimConfig {
  Timestamp latency_ns = 500'000;
  double maker_fee_bps = 0.0;
  double tick_size = 0.01;
};

struct BookConfig {
  Price min_price = 0;
  Price max_price = 1'000'000;
};

struct BacktestConfig {
  Timestamp sample_interval_ns = 1'000'000'000;
  Timestamp markout_ns = 1'000'000'000;
};

struct AppConfig {
  StrategyConfig strategy;
  RiskConfig risk;
  SimConfig sim;
  BookConfig book;
  BacktestConfig backtest;
};

AppConfig load_config(const std::string& path);
AppConfig parse_config(const std::string& text);

void apply_override(AppConfig& cfg, const std::string& assignment);

void validate(const AppConfig& cfg);

}
