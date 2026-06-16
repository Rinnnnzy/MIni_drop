#pragma once
#include <deque>
#include <map>
#include <mutex>
#include <string>

#include <grpcpp/grpcpp.h>
#include "control.grpc.pb.h"
#include "hotmethod.pb.h"

namespace dropd {

// ControlServiceImpl 实现 apiserver -> drop_server 的控制面接口（Day 4 才会被真正调用），
// 同时维护按 Agent IP 分组的任务队列 —— 这是 HealthCheckService 心跳时取任务的数据来源。
//
// 设计要点：tasks_ 会被两个不同的调用方并发访问：
//   - CreateTask()（未来由 apiserver 的 gRPC 调用触发）往里面"塞"任务
//   - PopTaskForAgent()（HealthCheckService 处理心跳时触发）从里面"取"任务
// 所以必须用 mutex_ 保护，不能假设只有单线程访问。
class ControlServiceImpl final : public control::Control::Service {
public:
  grpc::Status CreateTask(grpc::ServerContext* context,
                           const control::CreateTaskRequest* request,
                           control::CreateTaskResponse* response) override;

  grpc::Status FetchData(grpc::ServerContext* context,
                          const control::FetchDataRequest* request,
                          control::FetchDataResponse* response) override;

  grpc::Status StatAgent(grpc::ServerContext* context,
                          const control::StatAgentRequest* request,
                          control::StatAgentResponse* response) override;

  // PopTaskForAgent 给指定 IP 的 Agent 弹出一个待执行任务。
  // 由 HealthCheckServiceImpl::Do() 在处理心跳时调用（非 gRPC 接口，普通成员函数）。
  // 返回 true 表示弹出了任务并写入 out，false 表示该 Agent 当前没有待执行任务。
  bool PopTaskForAgent(const std::string& ip, hotmethod::TaskDesc* out);

private:
  std::mutex mutex_;
  std::map<std::string, std::deque<hotmethod::TaskDesc>> tasks_; // key = 目标 Agent IP
};

} // namespace dropd
