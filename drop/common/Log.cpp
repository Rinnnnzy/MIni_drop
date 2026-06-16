#include "Log.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace dropd {

Log::Level Log::min_level_ = Log::Level::INFO;

namespace {
// 保护标准输出，避免多线程（心跳线程 + 工作线程）同时打印导致一行日志被截断交错。
std::mutex g_log_mutex;

const char* LevelName(Log::Level level) {
  switch (level) {
    case Log::Level::DEBUG: return "debug";
    case Log::Level::INFO:  return "info";
    case Log::Level::WARN:  return "warn";
    case Log::Level::ERROR: return "error";
  }
  return "info";
}

Log::Level ParseLevel(const std::string& s) {
  if (s == "debug") return Log::Level::DEBUG;
  if (s == "warn")  return Log::Level::WARN;
  if (s == "error") return Log::Level::ERROR;
  return Log::Level::INFO;
}
} // namespace

void Log::Init(const std::string& level) {
  min_level_ = ParseLevel(level);
}

void Log::Write(Level level, const std::string& msg) {
  // enum class 的比较运算符默认按声明顺序比较底层整数值，
  // DEBUG=0 < INFO=1 < WARN=2 < ERROR=3，所以这里可以直接 <
  if (level < min_level_) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_log_mutex);

  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf{};
  gmtime_r(&now_c, &tm_buf); // 用 UTC 时间，避免容器时区配置不一致导致日志时间错乱

  // 输出单行 JSON，字段名对齐 Go 端 zap 日志的习惯（time/level/msg），方便统一采集
  std::cout << "{\"time\":\"" << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ")
            << "\",\"level\":\"" << LevelName(level)
            << "\",\"msg\":\"" << msg << "\"}" << std::endl;
}

void Log::Debug(const std::string& msg) { Write(Level::DEBUG, msg); }
void Log::Info(const std::string& msg)  { Write(Level::INFO, msg); }
void Log::Warn(const std::string& msg)  { Write(Level::WARN, msg); }
void Log::Error(const std::string& msg) { Write(Level::ERROR, msg); }

} // namespace dropd
