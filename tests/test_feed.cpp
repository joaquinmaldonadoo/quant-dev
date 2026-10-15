#include <gtest/gtest.h>

#include <thread>

#include "hft/csv_feed.hpp"
#include "hft/spsc_queue.hpp"

using namespace hft;

static std::string data_path(const char* name) { return std::string(HFT_TEST_DATA_DIR) + "/" + name; }

TEST(ParseLine, LineaValida) {
  const auto ev = parse_event_line("123,T,A,99,2");
  EXPECT_EQ(ev.ts, 123);
  EXPECT_EQ(ev.type, EventType::Trade);
  EXPECT_EQ(ev.side, Side::Ask);
  EXPECT_EQ(ev.price, 99);
  EXPECT_EQ(ev.qty, 2);
}

TEST(ParseLine, LineasInvalidas) {
  EXPECT_THROW(parse_event_line("123,T,A,99"), std::runtime_error);
  EXPECT_THROW(parse_event_line("123,T,A,99,2,7"), std::runtime_error);
  EXPECT_THROW(parse_event_line("abc,T,A,99,2"), std::runtime_error);
  EXPECT_THROW(parse_event_line("123,Q,A,99,2"), std::runtime_error);
  EXPECT_THROW(parse_event_line("123,B,X,99,2"), std::runtime_error);
  EXPECT_THROW(parse_event_line("123,B,B,99,-2"), std::runtime_error);
  EXPECT_THROW(parse_event_line("123,T,B,99,0"), std::runtime_error);
  EXPECT_THROW(parse_event_line("123,B,B,9.5,2"), std::runtime_error);
}

TEST(CsvFeed, LeeTodoEnOrden) {
  CsvFeed feed(data_path("small.csv"), 2);
  const auto expected = load_events_csv(data_path("small.csv"));
  ASSERT_EQ(expected.size(), 5u);
  MarketEvent ev;
  std::size_t i = 0;
  while (feed.next(ev)) {
    ASSERT_LT(i, expected.size());
    EXPECT_EQ(ev.ts, expected[i].ts);
    EXPECT_EQ(ev.price, expected[i].price);
    EXPECT_EQ(ev.qty, expected[i].qty);
    ++i;
  }
  EXPECT_EQ(i, expected.size());
  EXPECT_FALSE(feed.next(ev));
}

TEST(CsvFeed, PropagaErroresDelThreadLector) {
  CsvFeed feed(data_path("bad_type.csv"));
  MarketEvent ev;
  EXPECT_TRUE(feed.next(ev));
  EXPECT_THROW(feed.next(ev), std::runtime_error);
}

TEST(CsvFeed, RechazaTimestampsDesordenados) {
  EXPECT_THROW(load_events_csv(data_path("bad_order.csv")), std::runtime_error);
}

TEST(CsvFeed, ArchivoInexistente) {
  EXPECT_THROW(CsvFeed("/no/existe.csv"), std::runtime_error);
  EXPECT_THROW(load_events_csv("/no/existe.csv"), std::runtime_error);
}

TEST(CsvFeed, DestruirAntesDeTerminarNoSeCuelga) {
  CsvFeed feed(data_path("small.csv"), 2);
  MarketEvent ev;
  EXPECT_TRUE(feed.next(ev));
}

TEST(SpscQueue, CapacidadPotenciaDeDos) {
  EXPECT_THROW(SpscQueue<int>(3), std::invalid_argument);
  EXPECT_NO_THROW(SpscQueue<int>(4));
}

TEST(SpscQueue, LlenaYVacia) {
  SpscQueue<int> q(4);
  int x = 0;
  EXPECT_FALSE(q.try_pop(x));
  for (int i = 0; i < 4; ++i) EXPECT_TRUE(q.try_push(i));
  EXPECT_FALSE(q.try_push(99));
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(q.try_pop(x));
    EXPECT_EQ(x, i);
  }
  EXPECT_FALSE(q.try_pop(x));
}

TEST(SpscQueue, DosThreadsMantienenElOrden) {
  SpscQueue<std::uint64_t> q(256);
  constexpr std::uint64_t n = 1'000'000;
  std::thread producer([&] {
    for (std::uint64_t i = 0; i < n; ++i) {
      while (!q.try_push(i)) std::this_thread::yield();
    }
  });
  std::uint64_t expected = 0, v = 0;
  while (expected < n) {
    if (q.try_pop(v)) {
      ASSERT_EQ(v, expected);
      ++expected;
    }
  }
  producer.join();
}
