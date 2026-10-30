#include <gtest/gtest.h>

#include "hft/exchange_sim.hpp"
#include "hft/order_manager.hpp"

using namespace hft;

namespace {

MarketEvent trade(Timestamp ts, Side aggressor, Price p, Qty q) {
  MarketEvent e;
  e.ts = ts;
  e.type = EventType::Trade;
  e.side = aggressor;
  e.price = p;
  e.qty = q;
  return e;
}

struct SimFixture : ::testing::Test {
  OrderBook book{0, 1000};
  ExchangeSim sim;
  std::vector<ExecReport> out;

  void SetUp() override {
    book.set_level(Side::Bid, 99, 10);
    book.set_level(Side::Ask, 101, 10);
  }
  void set(Side s, Price p, Qty q, Timestamp ts = 0) {
    const Qty old = book.set_level(s, p, q);
    sim.on_book_update(s, p, old, q, book, ts, out);
  }
  Qty filled() const {
    Qty f = 0;
    for (const auto& r : out) f += r.type == ReportType::Fill ? r.qty : 0;
    return f;
  }
};

}

TEST_F(SimFixture, QuedaDetrasDeLaColaVisible) {
  sim.on_request({RequestType::New, 1, Side::Bid, 99, 2}, book, 0, out);
  ASSERT_EQ(out.back().type, ReportType::Accepted);
  EXPECT_DOUBLE_EQ(sim.queue_ahead(1), 10);
}

TEST_F(SimFixture, TradesConsumenPrimeroLaColaDeAdelante) {
  sim.on_request({RequestType::New, 1, Side::Bid, 99, 2}, book, 0, out);
  out.clear();
  sim.on_trade(trade(1, Side::Ask, 99, 7), out);
  EXPECT_EQ(filled(), 0);
  EXPECT_DOUBLE_EQ(sim.queue_ahead(1), 3);
  set(Side::Bid, 99, 3, 1);
  EXPECT_DOUBLE_EQ(sim.queue_ahead(1), 3);
  sim.on_trade(trade(2, Side::Ask, 99, 4), out);
  EXPECT_EQ(filled(), 1);
  ASSERT_EQ(out.back().type, ReportType::Fill);
  EXPECT_EQ(out.back().leaves, 1);
}

TEST_F(SimFixture, CancelacionesAdelantanLaColaEnProporcion) {
  sim.on_request({RequestType::New, 1, Side::Bid, 99, 1}, book, 0, out);
  set(Side::Bid, 99, 15);
  EXPECT_DOUBLE_EQ(sim.queue_ahead(1), 10);
  set(Side::Bid, 99, 12);
  EXPECT_DOUBLE_EQ(sim.queue_ahead(1), 8);
}

TEST_F(SimFixture, TradeQueAtraviesaNuestroPrecioNosLlena) {
  sim.on_request({RequestType::New, 1, Side::Ask, 102, 3}, book, 0, out);
  out.clear();
  sim.on_trade(trade(1, Side::Bid, 103, 1), out);
  EXPECT_EQ(filled(), 3);
  EXPECT_EQ(sim.resting_count(), 0u);
}

TEST_F(SimFixture, MejorarElPrecioDejaColaVacia) {
  sim.on_request({RequestType::New, 1, Side::Bid, 100, 1}, book, 0, out);
  EXPECT_DOUBLE_EQ(sim.queue_ahead(1), 0);
  out.clear();
  sim.on_trade(trade(1, Side::Ask, 99, 1), out);
  EXPECT_EQ(filled(), 1);
}

TEST_F(SimFixture, BookQueLlegaANuestroPrecioNosLlena) {
  sim.on_request({RequestType::New, 1, Side::Bid, 100, 1}, book, 0, out);
  out.clear();
  set(Side::Ask, 100, 4, 5);
  EXPECT_EQ(filled(), 1);
  EXPECT_EQ(out.back().exchange_ts, 5);
}

TEST_F(SimFixture, PostOnlyRechazaLoQueCruza) {
  sim.on_request({RequestType::New, 1, Side::Bid, 101, 1}, book, 0, out);
  EXPECT_EQ(out.back().type, ReportType::Rejected);
  sim.on_request({RequestType::New, 2, Side::Ask, 99, 1}, book, 0, out);
  EXPECT_EQ(out.back().type, ReportType::Rejected);
  EXPECT_EQ(sim.resting_count(), 0u);
}

TEST_F(SimFixture, CancelarYCancelarAlgoQueNoExiste) {
  sim.on_request({RequestType::New, 1, Side::Bid, 99, 1}, book, 0, out);
  sim.on_request({RequestType::Cancel, 1, Side::Bid, 99, 0}, book, 1, out);
  EXPECT_EQ(out.back().type, ReportType::Cancelled);
  sim.on_request({RequestType::Cancel, 1, Side::Bid, 99, 0}, book, 2, out);
  EXPECT_EQ(out.back().type, ReportType::CancelRejected);
}

TEST_F(SimFixture, TradeDelOtroLadoNoNosToca) {
  sim.on_request({RequestType::New, 1, Side::Bid, 99, 1}, book, 0, out);
  out.clear();
  sim.on_trade(trade(1, Side::Bid, 101, 50), out);
  EXPECT_EQ(filled(), 0);
}

namespace {

struct OmsFixture : ::testing::Test {
  StrategyConfig sc;
  RiskConfig rc;
  std::vector<OrderRequest> out;

  OmsFixture() {
    sc.min_requote_ns = 100;
    rc.max_orders_per_sec = 1000;
  }
  static QuotePair quotes(Price b, Price a, bool bid_on = true, bool ask_on = true) {
    QuotePair q;
    q.bid = {bid_on, b, 1};
    q.ask = {ask_on, a, 1};
    return q;
  }
};

}

TEST_F(OmsFixture, MandaAltasYNoRepiteSiNoCambia) {
  OrderManager oms(sc);
  RiskManager risk(rc);
  oms.on_quotes(quotes(99, 101), 0, risk, out);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].type, RequestType::New);
  EXPECT_EQ(out[0].side, Side::Bid);
  EXPECT_EQ(out[1].side, Side::Ask);
  out.clear();
  oms.on_quotes(quotes(99, 101), 1000, risk, out);
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(oms.open_qty(Side::Bid), 1);
}

TEST_F(OmsFixture, CambiarPrecioCancelaYReemplaza) {
  OrderManager oms(sc);
  RiskManager risk(rc);
  oms.on_quotes(quotes(99, 101), 0, risk, out);
  const OrderId first_bid = out[0].id;
  out.clear();
  oms.on_quotes(quotes(98, 101), 1000, risk, out);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].type, RequestType::Cancel);
  EXPECT_EQ(out[0].id, first_bid);
  EXPECT_EQ(out[1].type, RequestType::New);
  EXPECT_EQ(out[1].price, 98);
  EXPECT_EQ(oms.open_qty(Side::Bid), 2);
  oms.on_report({ReportType::Cancelled, first_bid, Side::Bid, 99, 0, 0, 0});
  EXPECT_EQ(oms.open_qty(Side::Bid), 1);
}

TEST_F(OmsFixture, RespetaElThrottleDeRecotizacion) {
  OrderManager oms(sc);
  RiskManager risk(rc);
  oms.on_quotes(quotes(99, 101), 0, risk, out);
  out.clear();
  oms.on_quotes(quotes(98, 102), 50, risk, out);
  EXPECT_TRUE(out.empty());
  oms.on_quotes(quotes(98, 102), 100, risk, out);
  EXPECT_EQ(out.size(), 4u);
}

TEST_F(OmsFixture, DesactivarUnLadoCancelaAunqueEsteEnThrottle) {
  OrderManager oms(sc);
  RiskManager risk(rc);
  oms.on_quotes(quotes(99, 101), 0, risk, out);
  out.clear();
  oms.on_quotes(quotes(99, 101, false, true), 1, risk, out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].type, RequestType::Cancel);
  EXPECT_EQ(oms.working(Side::Bid), nullptr);
}

TEST_F(OmsFixture, FillsActualizanPosicionYLiberanLaOrden) {
  OrderManager oms(sc);
  RiskManager risk(rc);
  QuotePair q = quotes(99, 101);
  q.bid.qty = 3;
  oms.on_quotes(q, 0, risk, out);
  const OrderId id = out[0].id;
  oms.on_report({ReportType::Accepted, id, Side::Bid, 99, 0, 3, 0});
  EXPECT_EQ(oms.working(Side::Bid)->state, OrderState::Live);
  oms.on_report({ReportType::Fill, id, Side::Bid, 99, 2, 1, 0});
  EXPECT_EQ(oms.position(), 2);
  EXPECT_EQ(oms.open_qty(Side::Bid), 1);
  oms.on_report({ReportType::Fill, id, Side::Bid, 99, 1, 0, 0});
  EXPECT_EQ(oms.position(), 3);
  EXPECT_EQ(oms.working(Side::Bid), nullptr);
  EXPECT_EQ(oms.stats().fills, 2);
}

TEST_F(OmsFixture, RechazoLiberaLaOrden) {
  OrderManager oms(sc);
  RiskManager risk(rc);
  oms.on_quotes(quotes(99, 101), 0, risk, out);
  oms.on_report({ReportType::Rejected, out[0].id, Side::Bid, 99, 0, 0, 0});
  EXPECT_EQ(oms.working(Side::Bid), nullptr);
  EXPECT_EQ(oms.stats().rejects, 1);
}

TEST_F(OmsFixture, CancelAll) {
  OrderManager oms(sc);
  RiskManager risk(rc);
  oms.on_quotes(quotes(99, 101), 0, risk, out);
  out.clear();
  oms.cancel_all(out);
  EXPECT_EQ(out.size(), 2u);
  EXPECT_EQ(oms.working(Side::Bid), nullptr);
  EXPECT_EQ(oms.working(Side::Ask), nullptr);
  EXPECT_EQ(oms.active_orders(), 2);
}

TEST_F(OmsFixture, SinLugarEnLaTasaNoCancelaSinReponer) {
  rc.max_orders_per_sec = 3;
  OrderManager oms(sc);
  RiskManager risk(rc);
  oms.on_quotes(quotes(99, 101), 0, risk, out);
  out.clear();
  oms.on_quotes(quotes(98, 101), 1000, risk, out);
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(oms.working(Side::Bid)->price, 99);
}
