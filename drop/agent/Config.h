#pragma once
#include <string>
#include <vector>

namespace dropd {

// MinioConfig 对应 config.json 里的 "minio" 字段，
// Day 4 上传 perf.data 时会用到这些凭证。
struct MinioConfig {
  std::string endpoint;
  std::string access_key;
  std::string secret_key;
  std::string bucket;
  bool use_ssl = false;
};

// AgentConfig 对应整个 config.json 的内容。
struct AgentConfig {
  // 支持多 Server 故障转移：依次尝试列表里的地址，直到注册成功为止。
  // Day 3 简化实现只用第一个地址，故障转移逻辑留待后续完善。
  std::vector<std::string> server_ips;
  std::string agent_uid;     // Agent 唯一 ID，心跳和注册时上报
  std::string agent_version; // 版本号，便于运维排查
  MinioConfig minio;
  std::string log_level = "info";
};

// LoadConfig 从 JSON 文件读取配置。
// 文件打不开或字段缺失（必填字段）都会抛出 std::runtime_error，
// 调用方（main.cpp）负责捕获并决定是否启动失败退出。
AgentConfig LoadConfig(const std::string& path);

} // namespace dropd
