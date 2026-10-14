#include "hft/order_book.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace hft {

OrderBook::OrderBook(Price min_price, Price max_price)
    : min_price_(min_price), max_price_(max_price) {
  if (max_price <= min_price) throw std::invalid_argument("rango de precios inválido para el book");
  const auto n = static_cast<std::size_t>(max_price - min_price + 1);
  bids_.assign(n, 0);
  asks_.assign(n, 0);
}

void OrderBook::check_range(Price p) const {
  if (p < min_price_ || p > max_price_) {
    throw std::out_of_range("precio " + std::to_string(p) + " fuera del rango del book [" +
                            std::to_string(min_price_) + ", " + std::to_string(max_price_) +
                            "]. Ampliá book.min_price / book.max_price");
  }
}

Price OrderBook::scan_bid_from(Price p) const {
  for (; p >= min_price_; --p) {
    if (bids_[index(p)] > 0) return p;
  }
  return kNoPrice;
}

Price OrderBook::scan_ask_from(Price p) const {
  for (; p <= max_price_; ++p) {
    if (asks_[index(p)] > 0) return p;
  }
  return kNoPrice;
}

Qty OrderBook::set_level(Side side, Price price, Qty qty) {
  check_range(price);
  if (qty < 0) throw std::invalid_argument("cantidad negativa en el book");
  const auto i = index(price);
  if (side == Side::Bid) {
    const Qty old = bids_[i];
    bids_[i] = qty;
    if (qty > 0) {
      if (best_bid_ == kNoPrice || price > best_bid_) best_bid_ = price;
    } else if (price == best_bid_) {
      best_bid_ = scan_bid_from(price - 1);
    }
    return old;
  }
  const Qty old = asks_[i];
  asks_[i] = qty;
  if (qty > 0) {
    if (best_ask_ == kNoPrice || price < best_ask_) best_ask_ = price;
  } else if (price == best_ask_) {
    best_ask_ = scan_ask_from(price + 1);
  }
  return old;
}

Qty OrderBook::qty_at(Side side, Price price) const {
  if (price < min_price_ || price > max_price_) return 0;
  return side == Side::Bid ? bids_[index(price)] : asks_[index(price)];
}

Qty OrderBook::best_qty(Side side) const {
  const Price p = side == Side::Bid ? best_bid_ : best_ask_;
  return p == kNoPrice ? 0 : qty_at(side, p);
}

int OrderBook::top_levels(Side side, int n, Level* out, Price max_scan_ticks) const {
  int found = 0;
  if (side == Side::Bid) {
    if (best_bid_ == kNoPrice) return 0;
    const Price stop = std::max(min_price_, best_bid_ - max_scan_ticks);
    for (Price p = best_bid_; p >= stop && found < n; --p) {
      const Qty q = bids_[index(p)];
      if (q > 0) out[found++] = {p, q};
    }
  } else {
    if (best_ask_ == kNoPrice) return 0;
    const Price stop = std::min(max_price_, best_ask_ + max_scan_ticks);
    for (Price p = best_ask_; p <= stop && found < n; ++p) {
      const Qty q = asks_[index(p)];
      if (q > 0) out[found++] = {p, q};
    }
  }
  return found;
}

void OrderBook::clear() {
  std::fill(bids_.begin(), bids_.end(), 0);
  std::fill(asks_.begin(), asks_.end(), 0);
  best_bid_ = kNoPrice;
  best_ask_ = kNoPrice;
}

}
