#include "hft/logger.hpp"

#include <chrono>
#include <ctime>
#include <stdexcept>

namespace hft {

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::set_output(const std::string& path) {
  std::lock_guard<std::mutex> lock(mu_);
  if (out_ != nullptr) {
    std::fclose(out_);
    out_ = nullptr;
  }
  if (path.empty()) return;
  out_ = std::fopen(path.c_str(), "a");
  if (out_ == nullptr) throw std::runtime_error("no se pudo abrir el log " + path);
}

void Logger::log(LogLevel level, const std::string& msg) {
  if (level < level_) return;
  static const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
  localtime_r(&now, &tm);
  char ts[32];
  std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
  std::lock_guard<std::mutex> lock(mu_);
  std::FILE* f = out_ != nullptr ? out_ : stderr;
  std::fprintf(f, "%s [%s] %s\n", ts, names[static_cast<int>(level)], msg.c_str());
  std::fflush(f);
}

Logger::~Logger() {
  if (out_ != nullptr) std::fclose(out_);
}

}
