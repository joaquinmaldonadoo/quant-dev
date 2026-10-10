#pragma once

#include <cstdio>
#include <mutex>
#include <string>

namespace hft {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

class Logger {
 public:
  static Logger& instance();

  void set_level(LogLevel level) { level_ = level; }
  LogLevel level() const { return level_; }
  void set_output(const std::string& path);

  void log(LogLevel level, const std::string& msg);

  ~Logger();

 private:
  Logger() = default;
  LogLevel level_ = LogLevel::Info;
  std::FILE* out_ = nullptr;
  std::mutex mu_;
};

inline void log_info(const std::string& m) { Logger::instance().log(LogLevel::Info, m); }
inline void log_warn(const std::string& m) { Logger::instance().log(LogLevel::Warn, m); }
inline void log_error(const std::string& m) { Logger::instance().log(LogLevel::Error, m); }
inline void log_debug(const std::string& m) { Logger::instance().log(LogLevel::Debug, m); }

}
