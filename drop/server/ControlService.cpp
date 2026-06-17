#include "ControlService.h"
#include "HotmethodService.h"

#include "Log.h"

namespace dropd {

ControlServiceImpl::ControlServiceImpl(HotmethodServiceImpl* hotmethod_svc)
    : hotmethod_svc_(hotmethod_svc) {}

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
    // Day 4：查询 HotmethodService 里存的采集结果（Agent 通过 NotifyResult 写入的）
    hotmethod::TaskResult result;
    bool found = hotmethod_svc_->GetResult(request->task_id(), &result);

    if (!found) {
        // 任务尚未完成（仍在 PENDING/RUNNING 状态）或不存在
        response->set_success(false);
        response->set_error_message("task not done yet or not found");
        return grpc::Status::OK;
    }

    if (!result.error_message().empty()) {
        // 任务失败：Agent 已上报错误原因
        response->set_success(false);
        response->set_error_message(result.error_message());
        return grpc::Status::OK;
    }

    // 任务成功：返回上传到 MinIO 的路径
    response->set_success(true);
    response->set_cos_key(result.cos_key());
    return grpc::Status::OK;
}

grpc::Status ControlServiceImpl::StatAgent(grpc::ServerContext* context,
                                            const control::StatAgentRequest* request,
                                            control::StatAgentResponse* response) {
    // TODO Day 5: 接入 Agent 上报的 PidStats 时序数据，返回真实 CPU/内存
    // 目前 HotmethodService 存的 TaskResult 里含 self_pstats，可以从最近的结果里取最后一条
    response->set_online(false);
    response->set_last_heartbeat("unknown");
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
