#include "HealthCheckService.h"

#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Log.h"

namespace dropd {

namespace {
constexpr int kOfflineThresholdSec = 30; // 超过 30 秒无心跳判离线（对应交付要求）
constexpr int kScanIntervalSec = 5;      // 后台扫描间隔，不需要扫得太频繁

std::string GetEnv(const char* name, const char* def) {
  const char* val = ::getenv(name);
  return (val && *val) ? std::string(val) : std::string(def);
}

// PostJSON 用 fork+exec curl 把 JSON POST 给 apiserver，与 InitAgentInfoService.cpp
// 的 NotifyApiserver 同一套模式，不引入额外依赖、不阻塞调用方太久。
void PostJSON(const std::string& path, const std::string& json) {
  std::string base_url = GetEnv("APISERVER_URL", "http://localhost:8191");
  std::string endpoint = base_url + path;

  pid_t pid = ::fork();
  if (pid == 0) {
    ::execlp("curl", "curl",
             "-s", "-X", "POST",
             endpoint.c_str(),
             "-H", "Content-Type: application/json",
             "-d", json.c_str(),
             nullptr);
    ::_exit(1);
  }
  if (pid > 0) {
    ::waitpid(pid, nullptr, 0);
  }
}

// 心跳 → 同步 apiserver：更新 last_seen / online，如果之前离线则在 Web 侧记一条"上线"审计日志。
// 复用 /internal/agent-register 端点 —— 它的语义本来就是"upsert 一条在线的 Agent 信息"。
void NotifyHeartbeat(const std::string& hostname, const std::string& ip_addr,
                     const std::string& version) {
  std::string json =
      "{\"hostname\":\"" + hostname +
      "\",\"ip_addr\":\"" + ip_addr +
      "\",\"version\":\"" + version + "\"}";
  PostJSON("/internal/agent-register", json);
}

// 离线 → 同步 apiserver：把 online 置 false，并记一条"离线"审计日志，Web 上 Agent 列表实时可见。
void NotifyOffline(const std::string& hostname, const std::string& ip_addr) {
  std::string json =
      "{\"hostname\":\"" + hostname +
      "\",\"ip_addr\":\"" + ip_addr + "\"}";
  PostJSON("/internal/agent-offline", json);
}

} // namespace

HealthCheckServiceImpl::HealthCheckServiceImpl(ControlServiceImpl* control_service)
    : control_service_(control_service) {
  running_ = true;
  scan_thread_ = std::thread(&HealthCheckServiceImpl::OfflineScanLoop, this);
}

HealthCheckServiceImpl::~HealthCheckServiceImpl() {
  running_ = false;
  if (scan_thread_.joinable()) {
    scan_thread_.join();
  }
}

grpc::Status HealthCheckServiceImpl::Do(grpc::ServerContext* context,
                                         const drop::HealthCheckRequest* request,
                                         drop::HealthCheckResponse* response) {
  const std::string& ip = request->ip_addr();

  bool was_offline = false;
  {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    AgentState& state = agents_[ip]; // 不存在则默认构造：online=true（见头文件注释）

    if (!state.online) {
      // 之前被后台扫描标记为离线，这次心跳重新连上了
      was_offline = true;
      Log::Info("[audit] agent recovered: ip=" + ip + " host=" + request->host_name());
    }

    state.last_seen = std::chrono::steady_clock::now();
    state.online = true;
  }

  // 每次心跳都同步给 apiserver，使 Postgres 的 last_seen 与 drop_server 内存状态一致。
  // 这样即使 drop_server 自身重启，apiserver 仍能凭 last_seen 独立判断离线（双重保险）。
  NotifyHeartbeat(request->host_name(), ip, request->agent_version());
  (void)was_offline; // 上线事件已经在 apiserver 的 /internal/agent-register 里写审计日志

  response->set_status(drop::HealthCheckResponse::SERVING);

  // 尝试从任务队列里取一个待执行任务塞进响应
  hotmethod::TaskDesc task;
  if (control_service_->PopTaskForAgent(ip, &task)) {
    response->set_pending(true);
    *response->mutable_task_desc() = task;
    Log::Info("dispatching task " + task.task_id() + " to agent " + ip);
  } else {
    response->set_pending(false);
  }

  return grpc::Status::OK;
}

void HealthCheckServiceImpl::OfflineScanLoop() {
  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(kScanIntervalSec));

    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(agents_mutex_);

    // 注意：CMakeLists.txt 设的是 C++14，这里不能用 C++17 的结构化绑定 (auto& [ip, state])
    for (auto& kv : agents_) {
      const std::string& ip = kv.first;
      AgentState& state = kv.second;

      if (!state.online) continue; // 已经是离线状态，不重复打日志

      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                         now - state.last_seen).count();
      if (elapsed > kOfflineThresholdSec) {
        state.online = false;
        // 审计日志：记录离线事件，对应交付要求"离线/恢复必须有审计日志"
        Log::Info("[audit] agent offline: ip=" + ip +
                  " last_seen_sec_ago=" + std::to_string(elapsed));
        NotifyOffline("", ip); // hostname 在此处未知，apiserver 侧按 ip_addr 更新即可
      }
    }
  }
}

} // namespace dropd
