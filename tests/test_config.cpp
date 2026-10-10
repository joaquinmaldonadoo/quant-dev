#include <gtest/gtest.h>

#include "hft/config.hpp"

using namespace hft;

TEST(Config, ParseaSeccionesYComentarios) {
  const auto cfg = parse_config(R"(
# comentario
[strategy]
gamma = 0.25   # comentario al final
k = 2.0
order_qty = 2

[risk]
max_inventory = 7
max_order_qty = 3

[sim]
latency_ns = 1000
)");
  EXPECT_DOUBLE_EQ(cfg.strategy.gamma, 0.25);
  EXPECT_DOUBLE_EQ(cfg.strategy.k, 2.0);
  EXPECT_EQ(cfg.strategy.order_qty, 2);
  EXPECT_EQ(cfg.risk.max_inventory, 7);
  EXPECT_EQ(cfg.sim.latency_ns, 1000);
  EXPECT_DOUBLE_EQ(cfg.strategy.tau_s, StrategyConfig{}.tau_s);
}

TEST(Config, ClaveDesconocidaTiraError) {
  EXPECT_THROW(parse_config("[strategy]\ngama = 0.1\n"), ConfigError);
}

TEST(Config, ValorInvalidoTiraError) {
  EXPECT_THROW(parse_config("[strategy]\ngamma = abc\n"), ConfigError);
  EXPECT_THROW(parse_config("[risk]\nmax_inventory = 3.5\n"), ConfigError);
}

TEST(Config, ValidaRangos) {
  EXPECT_THROW(parse_config("[strategy]\ngamma = 0\n"), ConfigError);
  EXPECT_THROW(parse_config("[book]\nmin_price = 10\nmax_price = 5\n"), ConfigError);
  EXPECT_THROW(parse_config("[strategy]\norder_qty = 10\n[risk]\nmax_order_qty = 5\n"), ConfigError);
}

TEST(Config, ClaveFueraDeSeccion) {
  EXPECT_THROW(parse_config("gamma = 1\n"), ConfigError);
}

TEST(Config, Override) {
  AppConfig cfg;
  apply_override(cfg, "strategy.beta_ofi=0.7");
  EXPECT_DOUBLE_EQ(cfg.strategy.beta_ofi, 0.7);
  EXPECT_THROW(apply_override(cfg, "strategy.nada=1"), ConfigError);
  EXPECT_THROW(apply_override(cfg, "sin_igual"), ConfigError);
}
