#include "hft/backtest_engine.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace hft {

BacktestEngine::BacktestEngine(const AppConfig& cfg, EngineOptions opts)
    : cfg_(cfg),
      opts_(std::move(opts)),
      book_(cfg.book.min_price, cfg.book.max_price),
      features_(cfg.strategy),
      strategy_(cfg.strategy),
      risk_(cfg.risk),
      oms_(cfg.strategy),
      portfolio_(cfg.sim.maker_fee_bps),
      metrics_(cfg.backtest.sample_interval_ns, cfg.backtest.markout_ns, cfg.sim.tick_size) {
  validate(cfg_);
  std::vector<Internal> storage;
  storage.reserve(1024);
  pending_ = decltype(pending_)(Later(), std::move(storage));
  reports_.reserve(64);
  requests_.reserve(64);
  if (!opts_.output_dir.empty()) metrics_.open_outputs(opts_.output_dir);
  if (!opts_.features_path.empty()) {
    const auto parent = std::filesystem::path(opts_.features_path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    features_out_.open(opts_.features_path);
    if (!features_out_) throw std::runtime_error("no se pudo crear " + opts_.features_path);
    features_out_ << "ts_ns,mid,micro_minus_mid,imbalance,ofi_norm,sigma,spread\n";
  }
}

void BacktestEngine::send_requests(Timestamp ts) {
  for (const auto& r : requests_) pending_.push({ts + cfg_.sim.latency_ns, seq_++, true, r, {}});
  requests_.clear();
}

void BacktestEngine::handle_exchange_reports(Timestamp ts) {
  const auto& f = features_.snapshot();
  for (const auto& r : reports_) {
    if (r.type == ReportType::Fill) {
      portfolio_.on_fill(r.side, r.price, r.qty);
      metrics_.on_fill(ts, r.side, r.price, r.qty, f.valid ? f.mid : static_cast<double>(r.price));
    }
    pending_.push({ts + cfg_.sim.latency_ns, seq_++, false, {}, r});
  }
  reports_.clear();
}

void BacktestEngine::decide(Timestamp ts) {
  if (!opts_.trade) return;
  if (risk_.halted()) {
    if (!cancelled_on_halt_) {
      oms_.cancel_all(requests_);
      cancelled_on_halt_ = true;
      send_requests(ts);
    }
    return;
  }
  const auto& f = features_.snapshot();
  if (!f.valid) return;
  const Qty pos = oms_.position();
  QuotePair q = strategy_.quote(f, pos, book_.best_bid(), book_.best_ask());
  risk_.filter(q, pos, oms_.open_qty(Side::Bid), oms_.open_qty(Side::Ask));
  oms_.on_quotes(q, ts, risk_, requests_);
  send_requests(ts);
}

void BacktestEngine::process_internal_until(Timestamp ts) {
  while (!pending_.empty() && pending_.top().ts <= ts) {
    const Internal ev = pending_.top();
    pending_.pop();
    if (ev.to_exchange) {
      exchange_.on_request(ev.req, book_, ev.ts, reports_);
      handle_exchange_reports(ev.ts);
    } else {
      oms_.on_report(ev.report);
      if (ev.report.type != ReportType::Accepted) decide(ev.ts);
    }
  }
}

void BacktestEngine::dump_features(Timestamp ts) {
  const auto& f = features_.snapshot();
  if (!features_out_.is_open() || !f.valid || ts < next_feature_ts_) return;
  features_out_ << ts << ',' << f.mid << ',' << (f.microprice - f.mid) << ',' << f.imbalance << ',' << f.ofi_norm
                << ',' << f.sigma << ',' << f.spread << '\n';
  next_feature_ts_ = (ts / opts_.features_every_ns + 1) * opts_.features_every_ns;
}

void BacktestEngine::on_market(const MarketEvent& ev) {
  ++summary_.market_events;
  if (ev.type == EventType::Trade) {
    ++summary_.trades_seen;
    exchange_.on_trade(ev, reports_);
    handle_exchange_reports(ev.ts);
  } else {
    const Qty old = book_.set_level(ev.side, ev.price, ev.qty);
    exchange_.on_book_update(ev.side, ev.price, old, ev.qty, book_, ev.ts, reports_);
    handle_exchange_reports(ev.ts);
    features_.on_book(book_, ev.ts);
  }

  const auto& f = features_.snapshot();
  if (f.valid) {
    const double eq = portfolio_.equity(f.mid);
    metrics_.on_state(ev.ts, f.mid, portfolio_.position(), eq, portfolio_.fees());
    risk_.on_equity(eq);
  }
  dump_features(ev.ts);
  decide(ev.ts);
}

Summary BacktestEngine::run(EventSource& source) {
  using Clock = std::chrono::steady_clock;
  MarketEvent ev;
  while (source.next(ev)) {
    if (opts_.measure_latency) {
      const auto t0 = Clock::now();
      process_internal_until(ev.ts);
      on_market(ev);
      latency_.add(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count());
    } else {
      process_internal_until(ev.ts);
      on_market(ev);
    }
  }

  Summary s = summary_;
  metrics_.finish(s);
  s.fees = portfolio_.fees();
  s.volume = portfolio_.volume();
  s.fills = portfolio_.fills();
  s.final_position = portfolio_.position();
  s.max_abs_position = portfolio_.max_abs_position();
  const auto& st = oms_.stats();
  s.orders_sent = st.news;
  s.cancels_sent = st.cancels;
  s.rejects = st.rejects;
  s.cancel_rejects = st.cancel_rejects;
  s.throttled = risk_.throttled();
  s.halted = risk_.halted();
  s.latency_measured = opts_.measure_latency && latency_.count() > 0;
  if (s.latency_measured) {
    s.latency_mean_ns = latency_.mean();
    s.latency_p50_ns = latency_.percentile(50);
    s.latency_p99_ns = latency_.percentile(99);
    s.latency_p999_ns = latency_.percentile(99.9);
    s.latency_max_ns = latency_.max();
  }
  if (features_out_.is_open()) features_out_.flush();
  return s;
}

}
