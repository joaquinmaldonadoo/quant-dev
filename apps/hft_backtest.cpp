
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "hft/backtest_engine.hpp"
#include "hft/config.hpp"
#include "hft/csv_feed.hpp"
#include "hft/logger.hpp"

namespace {

void usage() {
  std::cerr << "uso: hft_backtest --config ARCHIVO --data ARCHIVO [--out DIR] [--set clave=valor]...\n"
               "                   [--features ARCHIVO] [--features-every-ns N] [--no-trade]\n"
               "                   [--no-latency] [--log ARCHIVO]\n";
}

}

int main(int argc, char** argv) {
  std::string config_path, data_path, log_path;
  std::vector<std::string> overrides;
  hft::EngineOptions opts;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "falta el valor de " << a << "\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--config") config_path = value();
    else if (a == "--data") data_path = value();
    else if (a == "--out") opts.output_dir = value();
    else if (a == "--set") overrides.push_back(value());
    else if (a == "--features") opts.features_path = value();
    else if (a == "--features-every-ns") opts.features_every_ns = std::stoll(value());
    else if (a == "--no-trade") opts.trade = false;
    else if (a == "--no-latency") opts.measure_latency = false;
    else if (a == "--log") log_path = value();
    else if (a == "-h" || a == "--help") {
      usage();
      return 0;
    } else {
      std::cerr << "argumento desconocido: " << a << "\n";
      usage();
      return 2;
    }
  }
  if (config_path.empty() || data_path.empty()) {
    usage();
    return 2;
  }

  try {
    if (!log_path.empty()) hft::Logger::instance().set_output(log_path);
    hft::AppConfig cfg = hft::load_config(config_path);
    for (const auto& o : overrides) hft::apply_override(cfg, o);

    hft::log_info("config: " + config_path + " datos: " + data_path);
    hft::BacktestEngine engine(cfg, opts);
    hft::CsvFeed feed(data_path);
    const hft::Summary s = engine.run(feed);
    const std::string json = hft::summary_to_json(s);

    if (!opts.output_dir.empty()) {
      std::ofstream(opts.output_dir + "/summary.json") << json;
      std::ofstream info(opts.output_dir + "/run_info.txt");
      info << "config=" << config_path << "\ndata=" << data_path << "\n";
      for (const auto& o : overrides) info << "set " << o << "\n";
    }
    std::cout << json;
    if (s.halted) hft::log_warn("el kill switch cortó la estrategia por drawdown");
    hft::log_info("listo: " + std::to_string(s.market_events) + " eventos, " + std::to_string(s.fills) + " fills");
  } catch (const std::exception& e) {
    hft::log_error(e.what());
    return 1;
  }
  return 0;
}
