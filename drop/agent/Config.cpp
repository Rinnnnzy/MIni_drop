#include "Config.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace dropd {

AgentConfig LoadConfig(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    throw std::runtime_error("cannot open config file: " + path);
  }

  nlohmann::json j;
  ifs >> j; // 解析失败（JSON格式错误）会抛出 nlohmann::json::parse_error，是 std::exception 的子类

  AgentConfig cfg;
  // .at() 在字段缺失时抛异常（必填字段），.value() 允许缺失并给默认值（选填字段）
  cfg.server_ips = j.at("server_ips").get<std::vector<std::string>>();
  cfg.agent_uid = j.at("agent_uid").get<std::string>();
  cfg.agent_version = j.at("agent_version").get<std::string>();
  cfg.log_level = j.value("log_level", "info");

  const auto& m = j.at("minio");
  cfg.minio.endpoint = m.at("endpoint").get<std::string>();
  cfg.minio.access_key = m.at("access_key").get<std::string>();
  cfg.minio.secret_key = m.at("secret_key").get<std::string>();
  cfg.minio.bucket = m.at("bucket").get<std::string>();
  cfg.minio.use_ssl = m.value("use_ssl", false);

  if (cfg.server_ips.empty()) {
    throw std::runtime_error("config error: server_ips must not be empty");
  }

  return cfg;
}

} // namespace dropd
