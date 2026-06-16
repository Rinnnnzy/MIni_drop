#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "init.grpc.pb.h"

#include "Config.h"
#include "HealthCheckChannel.h"
#include "Log.h"

namespace {

std::atomic<bool> g_shutdown{false};
void HandleSignal(int) { g_shutdown = true; }

// RegisterToServer 在启动时调用一次 InitAgentInfo.RegisterAgent，
// 把自己的身份信息告知 drop_server（对应复刻指南里的 Agent 注册步骤）。
// 注册失败不阻塞启动 —— 心跳本身也会让 Server 感知到 Agent 的存在，
// 注册只是让 Server 提前拿到 hostname/version 等元信息。
void RegisterToServer(const std::string& server_addr,
                       const dropd::AgentConfig& cfg,
                       const std::string& host_name,
                       const std::string& ip_addr) {
  auto channel = grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials());
  auto stub = init_svc::InitAgentInfo::NewStub(channel);

  init_svc::RegisterAgentRequest req;
  req.set_host_name(host_name);
  req.set_ip_addr(ip_addr);
  req.set_uid(cfg.agent_uid);
  req.set_agent_version(cfg.agent_version);

  init_svc::RegisterAgentResponse resp;
  grpc::ClientContext ctx;
  ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

  grpc::Status status = stub->RegisterAgent(&ctx, req, &resp);
  if (!status.ok() || !resp.success()) {
    dropd::Log::Warn("agent registration failed, will rely on heartbeat instead: " +
                      status.error_message());
  } else {
    dropd::Log::Info("agent registered with server " + server_addr);
  }
}

} // namespace

int main(int argc, char** argv) {
  std::string config_path = "/etc/drop/config.json";
  if (argc > 1) {
    config_path = argv[1];
  }

  dropd::AgentConfig cfg;
  try {
    cfg = dropd::LoadConfig(config_path);
  } catch (const std::exception& e) {
    std::cerr << "failed to load config: " << e.what() << std::endl;
    return 1;
  }

  dropd::Log::Init(cfg.log_level);
  dropd::Log::Info("drop_agent starting, uid=" + cfg.agent_uid);

  // Day 3 简化版：直接用配置里的第一个 Server 地址。
  // 多 Server 故障转移（依次尝试直到注册成功）留待后续完善。
  const std::string& server_addr = cfg.server_ips[0];

  // host_name / ip_addr 这里先写占位值，Day 4 补充真实获取逻辑
  // （读 /etc/hostname、解析本机网卡地址，而不是硬编码）。
  const std::string host_name = "agent-host";
  const std::string ip_addr = "0.0.0.0";

  RegisterToServer(server_addr, cfg, host_name, ip_addr);

  dropd::HealthCheckChannel hc(server_addr, host_name, ip_addr,
                                cfg.agent_uid, cfg.agent_version);
  hc.Start();

  // 优雅退出：收到 SIGINT/SIGTERM 时跳出主循环，调用 hc.Stop() 等心跳线程收尾
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  dropd::Log::Info("drop_agent running, press Ctrl+C to stop");
  while (!g_shutdown) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  dropd::Log::Info("drop_agent shutting down");
  hc.Stop();
  return 0;
}
