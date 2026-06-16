#include "HealthCheckChannel.h"

#include <chrono>

#include "Log.h"

namespace dropd {

HealthCheckChannel::HealthCheckChannel(const std::string& server_addr,
                                        const std::string& host_name,
                                        const std::string& ip_addr,
                                        const std::string& uid,
                                        const std::string& agent_version)
    : server_addr_(server_addr),
      host_name_(host_name),
      ip_addr_(ip_addr),
      uid_(uid),
      agent_version_(agent_version) {
  // InsecureChannelCredentials：Day 3 内部网络先不上 TLS，简化调试。
  // 生产环境应该换成证书认证，复刻指南里也提到了证书路径要用绝对路径的坑。
  auto channel = grpc::CreateChannel(server_addr_, grpc::InsecureChannelCredentials());
  stub_ = drop::HealthCheck::NewStub(channel);
}

HealthCheckChannel::~HealthCheckChannel() {
  Stop();
}

void HealthCheckChannel::Start() {
  running_ = true;
  worker_ = std::thread(&HealthCheckChannel::Run, this);
}

void HealthCheckChannel::Stop() {
  // exchange(false) 保证多次调用 Stop() 是安全的：只有第一次调用会真正执行 join
  if (running_.exchange(false) && worker_.joinable()) {
    queue_cv_.notify_all(); // 唤醒可能阻塞在 PopTask() 里的工作线程，让它检测到 running_=false 后退出
    worker_.join();
  }
}

void HealthCheckChannel::Run() {
  Log::Info("heartbeat thread started, server=" + server_addr_);

  while (running_) {
    drop::HealthCheckRequest req;
    req.set_host_name(host_name_);
    req.set_ip_addr(ip_addr_);
    req.set_uid(uid_);
    req.set_agent_version(agent_version_);
    // TODO Day 4: 填充 self_pstats / children_pstats（读 /proc 实现自监控后补上）

    drop::HealthCheckResponse resp;
    grpc::ClientContext ctx;
    // 心跳超时设短一点（3秒），避免网络抖动时把下一次心跳也拖慢
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));

    grpc::Status status = stub_->Do(&ctx, req, &resp);

    if (!status.ok()) {
      // 网络错误或 Server 暂时不可达：打日志但不退出循环，下一轮 1 秒后继续重试
      Log::Warn("heartbeat failed: " + status.error_message());
    } else {
      if (resp.pending()) {
        // Server 派了任务过来：只入队，绝不在心跳线程里直接执行
        {
          std::lock_guard<std::mutex> lock(queue_mutex_);
          task_queue_.push(resp.task_desc());
        }
        queue_cv_.notify_one();
        Log::Info("received task: " + resp.task_desc().task_id());
      }
    }

    // 1Hz：每秒一次心跳，用 sleep_for 足够满足复刻指南的要求
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  Log::Info("heartbeat thread stopped");
}

bool HealthCheckChannel::PopTask(hotmethod::TaskDesc* out) {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  // 等待条件：队列非空，或者 channel 已停止（避免永久阻塞）
  queue_cv_.wait(lock, [this] { return !task_queue_.empty() || !running_; });

  if (task_queue_.empty()) {
    return false; // 走到这里说明是因为 running_=false 被唤醒，且队列确实空了
  }

  *out = task_queue_.front();
  task_queue_.pop();
  return true;
}

} // namespace dropd
