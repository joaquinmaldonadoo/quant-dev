#pragma once

#include <cstdint>
#include <limits>

namespace hft {

using Price = std::int64_t;
using Qty = std::int64_t;
using Timestamp = std::int64_t;
using OrderId = std::uint64_t;

inline constexpr Price kNoPrice = std::numeric_limits<Price>::min();

enum class Side : std::uint8_t { Bid = 0, Ask = 1 };

constexpr Side opposite(Side s) { return s == Side::Bid ? Side::Ask : Side::Bid; }
constexpr int sign(Side s) { return s == Side::Bid ? 1 : -1; }

enum class EventType : std::uint8_t { Book = 0, Trade = 1 };

struct MarketEvent {
  Timestamp ts = 0;
  Price price = 0;
  Qty qty = 0;
  EventType type = EventType::Book;
  Side side = Side::Bid;
};

}
