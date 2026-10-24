#include <gtest/gtest.h>

#include <cmath>

#include "hft/avellaneda_stoikov.hpp"
#include "hft/portfolio.hpp"
#include "hft/risk_manager.hpp"

using namespace hft;

namespace {

StrategyConfig as_cfg() {
  StrategyConfig c;
  c.gamma = 0.1;
  c.k = 1.5;
  c.tau_s = 10;
  c.order_qty = 1;
  return c;
}

FeatureSnapshot snap(double mid, double sigma, double alpha = 0) {
  FeatureSnapshot f;
  f.mid = mid;
  f.sigma = sigma;
  f.alpha = alpha;
  f.valid = true;
  return f;
}

}

TEST(AvellanedaStoikov, FormulasDelPaper) {
  AvellanedaStoikov as(as_cfg());
  EXPECT_NEAR(as.reservation_price(100, 0, 2, 0.5), 99.5, 1e-12);
  EXPECT_NEAR(as.half_spread(0.5), 0.125 + std::log(1 + 0.1 / 1.5) / 0.1, 1e-12);
}

TEST(AvellanedaStoikov, SinInventarioCotizaSimetrico) {
  AvellanedaStoikov as(as_cfg());
  const auto q = as.quote(snap(100, 0.5), 0, 99, 101);
  ASSERT_TRUE(q.bid.active && q.ask.active);
  EXPECT_DOUBLE_EQ(q.reservation, 100);
  EXPECT_EQ(100 - q.bid.price, q.ask.price - 100);
  EXPECT_LT(q.bid.price, q.ask.price);
}

TEST(AvellanedaStoikov, InventarioLargoBajaLasCotizaciones) {
  AvellanedaStoikov as(as_cfg());
  const auto flat = as.quote(snap(100, 1.0), 0, 90, 110);
  const auto lng = as.quote(snap(100, 1.0), 5, 90, 110);
  const auto sht = as.quote(snap(100, 1.0), -5, 90, 110);
  EXPECT_LT(lng.reservation, flat.reservation);
  EXPECT_GT(sht.reservation, flat.reservation);
  EXPECT_LE(lng.ask.price, flat.ask.price);
  EXPECT_GE(sht.bid.price, flat.bid.price);
}

TEST(AvellanedaStoikov, AlphaCorreElPrecioDeReserva) {
  AvellanedaStoikov as(as_cfg());
  const auto q = as.quote(snap(100, 0.5, 1.5), 0, 90, 110);
  EXPECT_DOUBLE_EQ(q.reservation, 101.5);
}

TEST(AvellanedaStoikov, SiempreQuedaPasivo) {
  AvellanedaStoikov as(as_cfg());
  auto f = snap(100, 0.5, 50);
  const auto q = as.quote(f, 0, 99, 101);
  EXPECT_EQ(q.bid.price, 100);
  EXPECT_GT(q.ask.price, 99);
  f.alpha = -50;
  const auto q2 = as.quote(f, 0, 99, 101);
  EXPECT_EQ(q2.ask.price, 100);
}

TEST(AvellanedaStoikov, FeaturesInvalidasNoCotiza) {
  AvellanedaStoikov as(as_cfg());
  FeatureSnapshot f;
  const auto q = as.quote(f, 0, 99, 101);
  EXPECT_FALSE(q.bid.active);
  EXPECT_FALSE(q.ask.active);
}

TEST(RiskManager, LimiteDeInventarioCuentaLoAbierto) {
  RiskConfig rc;
  rc.max_inventory = 3;
  rc.max_order_qty = 5;
  RiskManager rm(rc);
  QuotePair q;
  q.bid = {true, 99, 1};
  q.ask = {true, 101, 1};
  rm.filter(q, 2, 0, 0);
  EXPECT_TRUE(q.bid.active);
  q.bid.active = true;
  rm.filter(q, 2, 1, 0);
  EXPECT_FALSE(q.bid.active);
  EXPECT_TRUE(q.ask.active);
  QuotePair q2;
  q2.bid = {true, 99, 1};
  q2.ask = {true, 101, 1};
  rm.filter(q2, -3, 0, 0);
  EXPECT_FALSE(q2.ask.active);
  EXPECT_TRUE(q2.bid.active);
}

TEST(RiskManager, RecortaElTamanio) {
  RiskConfig rc;
  rc.max_order_qty = 2;
  rc.max_inventory = 100;
  RiskManager rm(rc);
  QuotePair q;
  q.bid = {true, 99, 7};
  q.ask = {true, 101, 7};
  rm.filter(q, 0, 0, 0);
  EXPECT_EQ(q.bid.qty, 2);
  EXPECT_EQ(q.ask.qty, 2);
}

TEST(RiskManager, TasaDeMensajes) {
  RiskConfig rc;
  rc.max_orders_per_sec = 3;
  RiskManager rm(rc);
  EXPECT_TRUE(rm.try_consume_messages(0, 2));
  EXPECT_FALSE(rm.try_consume_messages(10, 2));
  EXPECT_TRUE(rm.try_consume_messages(10, 1));
  EXPECT_FALSE(rm.try_consume_messages(500'000'000));
  EXPECT_EQ(rm.throttled(), 2);
  EXPECT_TRUE(rm.try_consume_messages(1'000'000'000, 2));
  EXPECT_FALSE(rm.try_consume_messages(1'000'000'001));
  EXPECT_FALSE(rm.try_consume_messages(1'000'000'001, 4));
}

TEST(RiskManager, KillSwitchPorDrawdown) {
  RiskConfig rc;
  rc.max_drawdown = 10;
  RiskManager rm(rc);
  rm.on_equity(0);
  rm.on_equity(25);
  rm.on_equity(16);
  EXPECT_FALSE(rm.halted());
  rm.on_equity(14.9);
  EXPECT_TRUE(rm.halted());
  QuotePair q;
  q.bid = {true, 99, 1};
  q.ask = {true, 101, 1};
  rm.filter(q, 0, 0, 0);
  EXPECT_FALSE(q.bid.active);
  EXPECT_FALSE(q.ask.active);
  rm.on_equity(100);
  EXPECT_TRUE(rm.halted());
}

TEST(Portfolio, PnlDeUnRoundTrip) {
  Portfolio p(0.0);
  p.on_fill(Side::Bid, 99, 2);
  p.on_fill(Side::Ask, 101, 2);
  EXPECT_EQ(p.position(), 0);
  EXPECT_DOUBLE_EQ(p.cash(), 4);
  EXPECT_DOUBLE_EQ(p.equity(500), 4);
  EXPECT_EQ(p.fills(), 2);
  EXPECT_EQ(p.max_abs_position(), 2);
}

TEST(Portfolio, MarcaAMidYFees) {
  Portfolio p(10.0);
  p.on_fill(Side::Bid, 1000, 1);
  EXPECT_DOUBLE_EQ(p.fees(), 1.0);
  EXPECT_DOUBLE_EQ(p.equity(1005), 4.0);
  Portfolio r(-1.0);
  r.on_fill(Side::Ask, 10000, 1);
  EXPECT_DOUBLE_EQ(r.fees(), -1.0);
  EXPECT_DOUBLE_EQ(r.cash(), 10001.0);
}
