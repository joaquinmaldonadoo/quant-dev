#include <gtest/gtest.h>

#include <cmath>

#include "hft/features.hpp"

using namespace hft;

namespace {

StrategyConfig base_cfg() {
  StrategyConfig c;
  c.imbalance_levels = 2;
  c.ofi_window_ns = 1000;
  c.vol_floor = 0.01;
  c.vol_halflife_s = 1.0;
  return c;
}

}

TEST(Ofi, CasosDelPaper) {
  EXPECT_DOUBLE_EQ(ofi_contribution(99, 5, 101, 5, 99, 8, 101, 5), 3);
  EXPECT_DOUBLE_EQ(ofi_contribution(99, 5, 101, 5, 99, 5, 101, 2), 3);
  EXPECT_DOUBLE_EQ(ofi_contribution(99, 5, 101, 5, 100, 4, 101, 5), 4);
  EXPECT_DOUBLE_EQ(ofi_contribution(99, 5, 101, 5, 98, 7, 101, 5), -5);
  EXPECT_DOUBLE_EQ(ofi_contribution(99, 5, 101, 5, 99, 5, 100, 3), -3);
  EXPECT_DOUBLE_EQ(ofi_contribution(99, 5, 101, 5, 99, 5, 102, 9), 5);
  EXPECT_DOUBLE_EQ(ofi_contribution(99, 5, 101, 5, 99, 5, 101, 5), 0);
}

TEST(Features, MicropriceEImbalance) {
  OrderBook b(0, 200);
  FeatureEngine fe(base_cfg());
  b.set_level(Side::Bid, 99, 3);
  b.set_level(Side::Bid, 98, 1);
  b.set_level(Side::Ask, 101, 1);
  b.set_level(Side::Ask, 103, 3);
  fe.on_book(b, 10);
  const auto& s = fe.snapshot();
  ASSERT_TRUE(s.valid);
  EXPECT_DOUBLE_EQ(s.mid, 100);
  EXPECT_DOUBLE_EQ(s.microprice, 100.5);
  EXPECT_DOUBLE_EQ(s.imbalance, 0);
  EXPECT_EQ(s.spread, 2);
}

TEST(Features, OfiConVentana) {
  OrderBook b(0, 200);
  FeatureEngine fe(base_cfg());
  b.set_level(Side::Bid, 99, 5);
  b.set_level(Side::Ask, 101, 5);
  fe.on_book(b, 0);
  EXPECT_DOUBLE_EQ(fe.snapshot().ofi, 0);
  b.set_level(Side::Bid, 99, 8);
  fe.on_book(b, 100);
  EXPECT_DOUBLE_EQ(fe.snapshot().ofi, 3);
  b.set_level(Side::Ask, 101, 3);
  fe.on_book(b, 500);
  EXPECT_DOUBLE_EQ(fe.snapshot().ofi, 5);
  fe.on_book(b, 1100);
  EXPECT_DOUBLE_EQ(fe.snapshot().ofi, 2);
  fe.on_book(b, 5000);
  EXPECT_DOUBLE_EQ(fe.snapshot().ofi, 0);
}

TEST(Features, VentanaDelOfiCrece) {
  OrderBook b(0, 200);
  auto cfg = base_cfg();
  cfg.ofi_window_ns = 1'000'000'000;
  FeatureEngine fe(cfg);
  b.set_level(Side::Bid, 99, 1);
  b.set_level(Side::Ask, 101, 5);
  fe.on_book(b, 0);
  for (int i = 1; i <= 5000; ++i) {
    b.set_level(Side::Bid, 99, 1 + i);
    fe.on_book(b, i);
  }
  EXPECT_DOUBLE_EQ(fe.snapshot().ofi, 5000);
}

TEST(Features, BookInvalido) {
  OrderBook b(0, 200);
  FeatureEngine fe(base_cfg());
  b.set_level(Side::Bid, 99, 1);
  fe.on_book(b, 0);
  EXPECT_FALSE(fe.snapshot().valid);
  b.set_level(Side::Ask, 99, 1);
  fe.on_book(b, 1);
  EXPECT_FALSE(fe.snapshot().valid);
}

TEST(Features, VolatilidadConvergeALaVerdadera) {
  OrderBook b(0, 200);
  auto cfg = base_cfg();
  cfg.vol_halflife_s = 2.0;
  FeatureEngine fe(cfg);
  Price bid = 99;
  for (int i = 0; i < 2000; ++i) {
    b.clear();
    b.set_level(Side::Bid, bid, 5);
    b.set_level(Side::Ask, bid + 2, 5);
    fe.on_book(b, static_cast<Timestamp>(i) * FeatureEngine::kVolSampleNs);
    bid += (i % 2 == 0) ? 1 : -1;
  }
  EXPECT_NEAR(fe.snapshot().sigma, std::sqrt(10.0), 1e-6);
}

TEST(Features, AlphaUsaLosBetasYSeRecorta) {
  OrderBook b(0, 200);
  auto cfg = base_cfg();
  cfg.beta_micro = 2.0;
  cfg.signal_clip_ticks = 0.6;
  FeatureEngine fe(cfg);
  b.set_level(Side::Bid, 99, 3);
  b.set_level(Side::Ask, 101, 1);
  fe.on_book(b, 0);
  EXPECT_DOUBLE_EQ(fe.snapshot().alpha, 0.6);
}
