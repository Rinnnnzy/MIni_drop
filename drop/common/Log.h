#pragma once
#include <string>

// dropd 命名空间专门用来装"我们自己手写的实现代码"，
// 和 protobuf 生成的 drop:: / hotmethod:: / control:: / init_svc:: 区分开，
// 避免和 proto 包名 "drop"（common.proto / healthcheck.proto 用的包名）混在一起。
namespace dropd {

// 极简的结构化日志封装：不依赖 glog（Dockerfile 里没装），
// 直接输出单行 JSON，方便后续接入日志采集系统。
class Log {
public:
  // Init 设置最低输出级别，低于这个级别的日志不打印。
  // level 可选：debug / info / warn / error
  static void Init(const std::string& level);

  static void Debug(const std::string& msg);
  static void Info(const std::string& msg);
  static void Warn(const std::string& msg);
  static void Error(const std::string& msg);

private:
  enum class Level { DEBUG, INFO, WARN, ERROR };

  static void Write(Level level, const std::string& msg);
  static Level min_level_;
};

} // namespace dropd
