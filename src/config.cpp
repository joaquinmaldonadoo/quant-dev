#include "hft/config.hpp"

#include <cstdlib>
#include <fstream>
#include <functional>
#include <sstream>

namespace hft {
namespace {

std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  const auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

double to_double(const std::string& key, const std::string& v) {
  char* end = nullptr;
  const double x = std::strtod(v.c_str(), &end);
  if (end == v.c_str() || *end != '\0') throw ConfigError("valor numérico inválido para " + key + ": " + v);
  return x;
}

std::int64_t to_int(const std::string& key, const std::string& v) {
  char* end = nullptr;
  const long long x = std::strtoll(v.c_str(), &end, 10);
  if (end == v.c_str() || *end != '\0') throw ConfigError("valor entero inválido para " + key + ": " + v);
  return static_cast<std::int64_t>(x);
}

using Setter = std::function<void(AppConfig&, const std::string& key, const std::string& value)>;

const std::map<std::string, Setter>& setters() {
  static const std::map<std::string, Setter> table = {
      {"strategy.gamma", [](AppConfig& c, auto& k, auto& v) { c.strategy.gamma = to_double(k, v); }},
      {"strategy.k", [](AppConfig& c, auto& k, auto& v) { c.strategy.k = to_double(k, v); }},
      {"strategy.tau_s", [](AppConfig& c, auto& k, auto& v) { c.strategy.tau_s = to_double(k, v); }},
      {"strategy.order_qty", [](AppConfig& c, auto& k, auto& v) { c.strategy.order_qty = to_int(k, v); }},
      {"strategy.vol_halflife_s", [](AppConfig& c, auto& k, auto& v) { c.strategy.vol_halflife_s = to_double(k, v); }},
      {"strategy.vol_floor", [](AppConfig& c, auto& k, auto& v) { c.strategy.vol_floor = to_double(k, v); }},
      {"strategy.beta_ofi", [](AppConfig& c, auto& k, auto& v) { c.strategy.beta_ofi = to_double(k, v); }},
      {"strategy.beta_micro", [](AppConfig& c, auto& k, auto& v) { c.strategy.beta_micro = to_double(k, v); }},
      {"strategy.ofi_window_ns", [](AppConfig& c, auto& k, auto& v) { c.strategy.ofi_window_ns = to_int(k, v); }},
      {"strategy.imbalance_levels", [](AppConfig& c, auto& k, auto& v) { c.strategy.imbalance_levels = static_cast<int>(to_int(k, v)); }},
      {"strategy.signal_clip_ticks", [](AppConfig& c, auto& k, auto& v) { c.strategy.signal_clip_ticks = to_double(k, v); }},
      {"strategy.min_requote_ns", [](AppConfig& c, auto& k, auto& v) { c.strategy.min_requote_ns = to_int(k, v); }},
      {"risk.max_inventory", [](AppConfig& c, auto& k, auto& v) { c.risk.max_inventory = to_int(k, v); }},
      {"risk.max_order_qty", [](AppConfig& c, auto& k, auto& v) { c.risk.max_order_qty = to_int(k, v); }},
      {"risk.max_drawdown", [](AppConfig& c, auto& k, auto& v) { c.risk.max_drawdown = to_double(k, v); }},
      {"risk.max_orders_per_sec", [](AppConfig& c, auto& k, auto& v) { c.risk.max_orders_per_sec = static_cast<int>(to_int(k, v)); }},
      {"sim.latency_ns", [](AppConfig& c, auto& k, auto& v) { c.sim.latency_ns = to_int(k, v); }},
      {"sim.maker_fee_bps", [](AppConfig& c, auto& k, auto& v) { c.sim.maker_fee_bps = to_double(k, v); }},
      {"sim.tick_size", [](AppConfig& c, auto& k, auto& v) { c.sim.tick_size = to_double(k, v); }},
      {"book.min_price", [](AppConfig& c, auto& k, auto& v) { c.book.min_price = to_int(k, v); }},
      {"book.max_price", [](AppConfig& c, auto& k, auto& v) { c.book.max_price = to_int(k, v); }},
      {"backtest.sample_interval_ns", [](AppConfig& c, auto& k, auto& v) { c.backtest.sample_interval_ns = to_int(k, v); }},
      {"backtest.markout_ns", [](AppConfig& c, auto& k, auto& v) { c.backtest.markout_ns = to_int(k, v); }},
  };
  return table;
}

void set_value(AppConfig& cfg, const std::string& full_key, const std::string& value) {
  const auto it = setters().find(full_key);
  if (it == setters().end()) throw ConfigError("clave desconocida: " + full_key);
  it->second(cfg, full_key, value);
}

}

AppConfig parse_config(const std::string& text) {
  AppConfig cfg;
  std::istringstream in(text);
  std::string line;
  std::string section;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    const auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    line = trim(line);
    if (line.empty()) continue;
    if (line.front() == '[') {
      if (line.back() != ']') throw ConfigError("sección mal cerrada en la línea " + std::to_string(line_no));
      section = trim(line.substr(1, line.size() - 2));
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) throw ConfigError("falta '=' en la línea " + std::to_string(line_no));
    if (section.empty()) throw ConfigError("clave fuera de sección en la línea " + std::to_string(line_no));
    set_value(cfg, section + "." + trim(line.substr(0, eq)), trim(line.substr(eq + 1)));
  }
  validate(cfg);
  return cfg;
}

AppConfig load_config(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw ConfigError("no se pudo abrir " + path);
  std::stringstream ss;
  ss << f.rdbuf();
  return parse_config(ss.str());
}

void apply_override(AppConfig& cfg, const std::string& assignment) {
  const auto eq = assignment.find('=');
  if (eq == std::string::npos) throw ConfigError("override sin '=': " + assignment);
  set_value(cfg, trim(assignment.substr(0, eq)), trim(assignment.substr(eq + 1)));
  validate(cfg);
}

void validate(const AppConfig& c) {
  auto require = [](bool ok, const char* msg) {
    if (!ok) throw ConfigError(msg);
  };
  require(c.strategy.gamma > 0, "strategy.gamma tiene que ser > 0");
  require(c.strategy.k > 0, "strategy.k tiene que ser > 0");
  require(c.strategy.tau_s > 0, "strategy.tau_s tiene que ser > 0");
  require(c.strategy.order_qty > 0, "strategy.order_qty tiene que ser > 0");
  require(c.strategy.vol_halflife_s > 0, "strategy.vol_halflife_s tiene que ser > 0");
  require(c.strategy.ofi_window_ns > 0, "strategy.ofi_window_ns tiene que ser > 0");
  require(c.strategy.imbalance_levels >= 1, "strategy.imbalance_levels tiene que ser >= 1");
  require(c.strategy.signal_clip_ticks >= 0, "strategy.signal_clip_ticks no puede ser negativo");
  require(c.risk.max_inventory > 0, "risk.max_inventory tiene que ser > 0");
  require(c.risk.max_order_qty >= c.strategy.order_qty, "risk.max_order_qty tiene que ser >= strategy.order_qty");
  require(c.risk.max_drawdown > 0, "risk.max_drawdown tiene que ser > 0");
  require(c.risk.max_orders_per_sec > 0, "risk.max_orders_per_sec tiene que ser > 0");
  require(c.sim.latency_ns >= 0, "sim.latency_ns no puede ser negativo");
  require(c.sim.tick_size > 0, "sim.tick_size tiene que ser > 0");
  require(c.book.max_price > c.book.min_price, "book.max_price tiene que ser mayor que book.min_price");
  require(c.backtest.sample_interval_ns > 0, "backtest.sample_interval_ns tiene que ser > 0");
  require(c.backtest.markout_ns > 0, "backtest.markout_ns tiene que ser > 0");
}

}
