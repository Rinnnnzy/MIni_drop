#include "HealthCheckService.h"

#include "Log.h"

namespace dropd {

namespace {
constexpr int kOfflineThresholdSec = 30; // 超过 30 秒无心跳判离线（对应交付要求）
constexpr int kScanIntervalSec = 5;      // 后台扫描间隔，不需要扫得太频繁
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

  {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    AgentState& state = agents_[ip]; // 不存在则默认构造：online=true（见头文件注释）

    if (!state.online) {
      // 之前被后台扫描标记为离线，这次心跳重新连上了
      Log::Info("[audit] agent recovered: ip=" + ip + " host=" + request->host_name());
    }

    state.last_seen = std::chrono::steady_clock::now();
    state.online = true;
  }

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
      }
    }
  }
}

} // namespace dropd
