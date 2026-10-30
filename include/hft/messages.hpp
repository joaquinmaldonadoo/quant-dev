#pragma once

#include "hft/types.hpp"

namespace hft {

enum class RequestType : std::uint8_t { New, Cancel };

struct OrderRequest {
  RequestType type = RequestType::New;
  OrderId id = 0;
  Side side = Side::Bid;
  Price price = 0;
  Qty qty = 0;
};

enum class ReportType : std::uint8_t {
  Accepted,
  Rejected,
  Fill,
  Cancelled,
  CancelRejected,
};

struct ExecReport {
  ReportType type = ReportType::Accepted;
  OrderId id = 0;
  Side side = Side::Bid;
  Price price = 0;
  Qty qty = 0;
  Qty leaves = 0;
  Timestamp exchange_ts = 0;
};

}
