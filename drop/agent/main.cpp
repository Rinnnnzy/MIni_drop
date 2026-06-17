#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>
#include "init.grpc.pb.h"

#include "Config.h"
#include "HealthCheckChannel.h"
#include "HotmethodChannel.h"
#include "Log.h"

namespace {

std::atomic<bool> g_shutdown{false};
void HandleSignal(int) { g_shutdown = true; }

// GetLocalIP 遍历本机网卡，返回第一个非 loopback 的 IPv4 地址。
// 找不到时回退到 "127.0.0.1"，保证 agent 不会上报 "0.0.0.0"。
std::string GetLocalIP() {
    struct ifaddrs* ifas = nullptr;
    if (getifaddrs(&ifas) != 0) return "127.0.0.1";

    std::string result = "127.0.0.1";
    for (auto* ifa = ifas; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (std::string(ifa->ifa_name) == "lo") continue; // 跳过 loopback
        char buf[INET_ADDRSTRLEN];
        auto* sin = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
        inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
        result = buf;
        break; // 取第一个非 loopback IPv4，足够 MVP 使用
    }

    freeifaddrs(ifas);
    return result;
}

// GetHostName 读取系统 hostname，失败时返回 "unknown"。
std::string GetHostName() {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0) return "unknown";
    return buf;
}

// RegisterToServer 在启动时调用一次 InitAgentInfo.RegisterAgent，
// 把自己的身份信息告知 drop_server。
// 注册失败不阻塞启动 —— 心跳本身也会让 Server 感知到 Agent 的存在。
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
        dropd::Log::Warn("agent registration failed, will rely on heartbeat: "
                          + status.error_message());
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

    const std::string& server_addr = cfg.server_ips[0];

    // 读取真实的 host_name 和 ip_addr：这两个值会随心跳上报给 drop_server，
    // drop_server 用 ip_addr 作为任务队列的 key——必须和 apiserver 创建任务时
    // 填写的 target_ip 一致，否则 agent 永远取不到任务。
    const std::string host_name = GetHostName();
    const std::string ip_addr   = GetLocalIP();

    RegisterToServer(server_addr, cfg, host_name, ip_addr);

    // ── 心跳线程：只负责发心跳 + 把任务塞进队列 ──────────────────────────────
    dropd::HealthCheckChannel hc(server_addr, host_name, ip_addr,
                                  cfg.agent_uid, cfg.agent_version);
    hc.Start();

    // ── 工作线程：从队列取任务，执行 perf → 上传 → NotifyResult ──────────────
    // Day 4 新增：HotmethodChannel 使用 Agent 自己 config.json 里的 MinIO 配置，
    // 不依赖 TaskDesc 里的 cos_config（简化 MVP 流程）。
    dropd::HotmethodChannel hmc(
        server_addr,
        &hc,
        cfg.minio.endpoint,
        cfg.minio.access_key,
        cfg.minio.secret_key,
        cfg.minio.bucket,
        cfg.minio.use_ssl
    );
    hmc.Start();

    // 优雅退出
    std::signal(SIGINT,  HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    dropd::Log::Info("drop_agent running (heartbeat + worker threads active)");
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    dropd::Log::Info("drop_agent shutting down");
    // 先停心跳线程（它的 Stop() 会 notify_all 唤醒工作线程的 PopTask）
    hc.Stop();
    // 再停工作线程（此时 PopTask 已被唤醒，Run 循环会退出）
    hmc.Stop();

    return 0;
}
