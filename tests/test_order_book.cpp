#include <gtest/gtest.h>

#include "hft/order_book.hpp"

using namespace hft;

TEST(OrderBook, ArrancaVacio) {
  OrderBook b(0, 100);
  EXPECT_EQ(b.best_bid(), kNoPrice);
  EXPECT_EQ(b.best_ask(), kNoPrice);
  EXPECT_FALSE(b.has_both_sides());
  EXPECT_FALSE(b.crossed());
}

TEST(OrderBook, MantieneElMejorPrecio) {
  OrderBook b(0, 100);
  b.set_level(Side::Bid, 48, 5);
  b.set_level(Side::Bid, 49, 3);
  b.set_level(Side::Bid, 47, 9);
  b.set_level(Side::Ask, 52, 4);
  b.set_level(Side::Ask, 51, 2);
  EXPECT_EQ(b.best_bid(), 49);
  EXPECT_EQ(b.best_ask(), 51);
  EXPECT_EQ(b.best_qty(Side::Bid), 3);
  EXPECT_EQ(b.best_qty(Side::Ask), 2);
  EXPECT_DOUBLE_EQ(b.mid(), 50.0);
  EXPECT_EQ(b.spread(), 2);
}

TEST(OrderBook, BuscaElSiguienteNivelCuandoSeVaciaElMejor) {
  OrderBook b(0, 100);
  b.set_level(Side::Bid, 45, 1);
  b.set_level(Side::Bid, 49, 3);
  b.set_level(Side::Ask, 51, 2);
  b.set_level(Side::Ask, 55, 7);
  b.set_level(Side::Bid, 49, 0);
  b.set_level(Side::Ask, 51, 0);
  EXPECT_EQ(b.best_bid(), 45);
  EXPECT_EQ(b.best_ask(), 55);
  b.set_level(Side::Bid, 45, 0);
  EXPECT_EQ(b.best_bid(), kNoPrice);
}

TEST(OrderBook, BorrarUnNivelQueNoEsElMejorNoCambiaElMejor) {
  OrderBook b(0, 100);
  b.set_level(Side::Bid, 45, 1);
  b.set_level(Side::Bid, 49, 3);
  b.set_level(Side::Bid, 45, 0);
  EXPECT_EQ(b.best_bid(), 49);
}

TEST(OrderBook, DevuelveLaCantidadAnterior) {
  OrderBook b(0, 100);
  EXPECT_EQ(b.set_level(Side::Ask, 60, 10), 0);
  EXPECT_EQ(b.set_level(Side::Ask, 60, 4), 10);
  EXPECT_EQ(b.qty_at(Side::Ask, 60), 4);
}

TEST(OrderBook, TopLevelsSaltaNivelesVacios) {
  OrderBook b(0, 100);
  b.set_level(Side::Bid, 49, 1);
  b.set_level(Side::Bid, 46, 2);
  b.set_level(Side::Bid, 40, 3);
  Level lv[5];
  ASSERT_EQ(b.top_levels(Side::Bid, 2, lv), 2);
  EXPECT_EQ(lv[0].price, 49);
  EXPECT_EQ(lv[1].price, 46);
  EXPECT_EQ(lv[1].qty, 2);
  EXPECT_EQ(b.top_levels(Side::Bid, 5, lv, 5), 2);
  EXPECT_EQ(b.top_levels(Side::Bid, 5, lv), 3);
}

TEST(OrderBook, DetectaBookCruzado) {
  OrderBook b(0, 100);
  b.set_level(Side::Bid, 50, 1);
  b.set_level(Side::Ask, 51, 1);
  EXPECT_FALSE(b.crossed());
  b.set_level(Side::Bid, 51, 1);
  EXPECT_TRUE(b.crossed());
}

TEST(OrderBook, ErroresDeRango) {
  OrderBook b(10, 20);
  EXPECT_THROW(b.set_level(Side::Bid, 9, 1), std::out_of_range);
  EXPECT_THROW(b.set_level(Side::Ask, 21, 1), std::out_of_range);
  EXPECT_THROW(b.set_level(Side::Ask, 15, -1), std::invalid_argument);
  EXPECT_EQ(b.qty_at(Side::Bid, 1000), 0);
  EXPECT_THROW(OrderBook(5, 5), std::invalid_argument);
}

TEST(OrderBook, ExtremosDelRango) {
  OrderBook b(10, 20);
  b.set_level(Side::Bid, 10, 1);
  b.set_level(Side::Ask, 20, 1);
  EXPECT_EQ(b.best_bid(), 10);
  EXPECT_EQ(b.best_ask(), 20);
  b.set_level(Side::Bid, 10, 0);
  b.set_level(Side::Ask, 20, 0);
  EXPECT_EQ(b.best_bid(), kNoPrice);
  EXPECT_EQ(b.best_ask(), kNoPrice);
}
