#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <grpcpp/grpcpp.h>
#include "healthcheck.grpc.pb.h"

#include "ControlService.h"

namespace dropd {

// HealthCheckServiceImpl 处理 Agent 心跳，承担3个职责：
//   1. 记录每个 Agent 的最后心跳时间
//   2. 心跳响应里携带待执行任务（从 ControlServiceImpl 的队列里取）
//   3. 后台线程定期扫描：超过 30 秒无心跳的 Agent 标记离线，并打审计日志；
//      重新收到心跳时如果之前是离线状态，也打一条"恢复"审计日志。
//      这对应交付要求："离线/恢复必须有审计日志"。
class HealthCheckServiceImpl final : public drop::HealthCheck::Service {
public:
  // control_service 用来取任务，生命周期由 main.cpp 保证（必须比本对象活得久），
  // 这里只持有裸指针，不拥有所有权。
  explicit HealthCheckServiceImpl(ControlServiceImpl* control_service);
  ~HealthCheckServiceImpl() override;

  grpc::Status Do(grpc::ServerContext* context,
                   const drop::HealthCheckRequest* request,
                   drop::HealthCheckResponse* response) override;

private:
  struct AgentState {
    std::chrono::steady_clock::time_point last_seen;
    bool online = true; // 默认 true：第一次心跳时不应该被当成"恢复"事件
  };

  void OfflineScanLoop(); // 后台线程主体：每 5 秒扫一次

  ControlServiceImpl* control_service_;

  std::mutex agents_mutex_;
  std::unordered_map<std::string, AgentState> agents_; // key = ip_addr

  std::thread scan_thread_;
  std::atomic<bool> running_{false};
};

} // namespace dropd
