#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "hft/backtest_engine.hpp"

using namespace hft;

namespace {

MarketEvent book_ev(Timestamp ts, Side s, Price p, Qty q) { return {ts, p, q, EventType::Book, s}; }
MarketEvent trade_ev(Timestamp ts, Side aggressor, Price p, Qty q) { return {ts, p, q, EventType::Trade, aggressor}; }

AppConfig test_cfg() {
  AppConfig c;
  c.book.min_price = 0;
  c.book.max_price = 1000;
  c.strategy.gamma = 0.1;
  c.strategy.k = 1.0;
  c.strategy.tau_s = 1.0;
  c.strategy.vol_floor = 0.1;
  c.strategy.order_qty = 1;
  c.strategy.min_requote_ns = 0;
  c.risk.max_inventory = 5;
  c.risk.max_order_qty = 1;
  c.sim.latency_ns = 10;
  c.backtest.sample_interval_ns = 100;
  c.backtest.markout_ns = 50;
  return c;
}

std::vector<MarketEvent> scenario() {
  return {
      book_ev(0, Side::Bid, 99, 10),
      book_ev(0, Side::Ask, 101, 10),
      book_ev(100, Side::Bid, 98, 5),
      trade_ev(200, Side::Ask, 99, 12),
      book_ev(200, Side::Bid, 99, 0),
      book_ev(300, Side::Bid, 99, 4),
      book_ev(400, Side::Ask, 101, 9),
      book_ev(500, Side::Ask, 101, 9),
  };
}

}

TEST(Engine, CotizaRecibeFillYActualizaPosicion) {
  BacktestEngine engine(test_cfg(), EngineOptions{});
  VectorSource src(scenario());
  const Summary s = engine.run(src);
  EXPECT_EQ(s.market_events, 8);
  EXPECT_EQ(s.trades_seen, 1);
  EXPECT_GE(s.orders_sent, 2);
  ASSERT_EQ(s.fills, 1);
  EXPECT_EQ(engine.portfolio().position(), 1);
  EXPECT_EQ(engine.oms().position(), 1);
  EXPECT_DOUBLE_EQ(engine.portfolio().cash(), -99);
}

TEST(Engine, OrdenNoLlegaAntesDeLaLatencia) {
  auto cfg = test_cfg();
  cfg.sim.latency_ns = 1000;
  BacktestEngine engine(cfg, EngineOptions{});
  VectorSource src(scenario());
  const Summary s = engine.run(src);
  EXPECT_EQ(s.fills, 0);
}

TEST(Engine, EsDeterministico) {
  auto run_once = [] {
    BacktestEngine engine(test_cfg(), EngineOptions{});
    VectorSource src(scenario());
    return summary_to_json(engine.run(src));
  };
  auto run_no_lat = [] {
    EngineOptions o;
    o.measure_latency = false;
    BacktestEngine engine(test_cfg(), o);
    VectorSource src(scenario());
    return summary_to_json(engine.run(src));
  };
  run_once();
  EXPECT_EQ(run_no_lat(), run_no_lat());
}

TEST(Engine, LimiteDeInventarioFrenaLasCompras) {
  auto cfg = test_cfg();
  cfg.risk.max_inventory = 1;
  std::vector<MarketEvent> evs = {book_ev(0, Side::Bid, 99, 1), book_ev(0, Side::Ask, 101, 1)};
  for (int i = 1; i <= 20; ++i) {
    evs.push_back(trade_ev(i * 100, Side::Ask, 98, 5));
    evs.push_back(book_ev(i * 100 + 1, Side::Ask, 101, 1));
  }
  BacktestEngine engine(cfg, EngineOptions{});
  VectorSource src(evs);
  const Summary s = engine.run(src);
  EXPECT_LE(s.max_abs_position, 2);
  EXPECT_LE(engine.portfolio().position(), 2);
}

TEST(Engine, SinTradingSoloCalculaFeatures) {
  const auto dir = std::filesystem::temp_directory_path() / "hft_test_features";
  std::filesystem::remove_all(dir);
  EngineOptions o;
  o.trade = false;
  o.features_path = (dir / "f.csv").string();
  o.features_every_ns = 100;
  BacktestEngine engine(test_cfg(), o);
  VectorSource src(scenario());
  const Summary s = engine.run(src);
  EXPECT_EQ(s.orders_sent, 0);
  std::ifstream f(o.features_path);
  std::string line;
  int lines = 0;
  while (std::getline(f, line)) ++lines;
  EXPECT_GE(lines, 4);
}

TEST(Engine, EscribeLosCsv) {
  const auto dir = std::filesystem::temp_directory_path() / "hft_test_out";
  std::filesystem::remove_all(dir);
  EngineOptions o;
  o.output_dir = dir.string();
  {
    BacktestEngine engine(test_cfg(), o);
    VectorSource src(scenario());
    engine.run(src);
  }
  EXPECT_TRUE(std::filesystem::exists(dir / "pnl.csv"));
  EXPECT_TRUE(std::filesystem::exists(dir / "fills.csv"));
  std::ifstream fills(dir / "fills.csv");
  std::string header, row;
  std::getline(fills, header);
  ASSERT_TRUE(std::getline(fills, row));
  EXPECT_EQ(row.substr(0, 4), "200,");
}

TEST(Metrics, Percentiles) {
  LatencyHistogram h;
  for (int i = 1; i <= 100; ++i) h.add(i * 10);
  EXPECT_EQ(h.count(), 100);
  EXPECT_EQ(h.percentile(50), 510);
  EXPECT_EQ(h.max(), 1000);
  h.add(10'000'000);
  EXPECT_EQ(h.percentile(100), LatencyHistogram::kBuckets * LatencyHistogram::kBucketNs);
}

TEST(Metrics, MarkoutYDrawdown) {
  Metrics m(100, 50, 1.0);
  m.on_state(0, 100, 0, 0, 0);
  m.on_fill(10, Side::Bid, 99, 1, 100);
  m.on_state(10, 100, 1, 1, 0);
  m.on_state(30, 97, 1, -2, 0);
  m.on_state(60, 98, 1, -1, 0);
  Summary s;
  m.finish(s);
  EXPECT_EQ(s.markouts, 1);
  EXPECT_DOUBLE_EQ(s.avg_markout, -1);
  EXPECT_DOUBLE_EQ(s.adverse_fill_ratio, 1);
  EXPECT_DOUBLE_EQ(s.max_drawdown, 3);
  EXPECT_DOUBLE_EQ(s.final_equity, -1);
}
