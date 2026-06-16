#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "healthcheck.grpc.pb.h" // 包含 drop::HealthCheck / HealthCheckRequest / HealthCheckResponse
#include "hotmethod.pb.h"        // 直接用到 hotmethod::TaskDesc，显式 include 更清晰

namespace dropd {

// HealthCheckChannel 在独立线程里以 1Hz 频率向 drop_server 发送心跳。
//
// 设计要点（易踩坑提醒）：
//   心跳线程绝不能被任务执行阻塞 —— 如果 perf 跑 10 秒，期间心跳线程也被占用，
//   Server 会因为收不到心跳误判 Agent 离线。所以这里的策略是：
//   心跳线程只负责"收到任务 -> 塞进队列 -> 立刻继续下一次心跳"，
//   真正执行任务的逻辑由另一个工作线程（HotmethodChannel，Day 4 实现）从队列里取走执行。
class HealthCheckChannel {
public:
  HealthCheckChannel(const std::string& server_addr,
                      const std::string& host_name,
                      const std::string& ip_addr,
                      const std::string& uid,
                      const std::string& agent_version);
  ~HealthCheckChannel();

  void Start(); // 启动心跳线程
  void Stop();  // 停止心跳线程（析构时会自动调用一次，重复调用安全）

  // PopTask 阻塞等待队列中出现任务，由 Day 4 的工作线程调用。
  // 返回 false 表示 channel 已经停止（用于工作线程优雅退出）。
  bool PopTask(hotmethod::TaskDesc* out);

private:
  void Run(); // 心跳循环主体，在独立线程里跑

  std::string server_addr_;
  std::string host_name_;
  std::string ip_addr_;
  std::string uid_;
  std::string agent_version_;

  std::unique_ptr<drop::HealthCheck::Stub> stub_;
  std::thread worker_;
  std::atomic<bool> running_{false};

  // 任务队列：心跳线程只"塞"，工作线程只"取"，两者通过队列解耦
  std::queue<hotmethod::TaskDesc> task_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
};

} // namespace dropd
