#include "ControlService.h"

#include "Log.h"

namespace dropd {

grpc::Status ControlServiceImpl::CreateTask(grpc::ServerContext* context,
                                             const control::CreateTaskRequest* request,
                                             control::CreateTaskResponse* response) {
  const std::string& ip = request->target_ip();

  {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_[ip].push_back(request->task_desc());
  }

  Log::Info("task queued for agent " + ip + ", task_id=" + request->task_desc().task_id());

  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status ControlServiceImpl::FetchData(grpc::ServerContext* context,
                                            const control::FetchDataRequest* request,
                                            control::FetchDataResponse* response) {
  // TODO Day 4: 接入真实的任务结果查询（结果由 NotifyResult 写入后，这里应该能查到 cos_key）
  response->set_success(false);
  response->set_error_message("not implemented yet (Day 4)");
  return grpc::Status::OK;
}

grpc::Status ControlServiceImpl::StatAgent(grpc::ServerContext* context,
                                            const control::StatAgentRequest* request,
                                            control::StatAgentResponse* response) {
  // TODO Day 4: 接入 Agent 自监控上报的 PidStats 数据，返回真实 CPU/内存占用
  response->set_online(false);
  return grpc::Status::OK;
}

bool ControlServiceImpl::PopTaskForAgent(const std::string& ip, hotmethod::TaskDesc* out) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = tasks_.find(ip);
  if (it == tasks_.end() || it->second.empty()) {
    return false;
  }

  *out = it->second.front();
  it->second.pop_front();
  return true;
}

} // namespace dropd
