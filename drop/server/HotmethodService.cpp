#include "HotmethodService.h"

#include "Log.h"

namespace dropd {

grpc::Status HotmethodServiceImpl::Collect(grpc::ServerContext* /*context*/,
                                            const hotmethod::Target* request,
                                            google::protobuf::Empty* /*response*/) {
    // Collect 是 Server 主动推任务给 Agent 的 RPC（备用路径）。
    // 当前架构的主路径是心跳响应带任务，Collect 在 Day 4 暂只做日志记录。
    // TODO Day 5: 把任务转发到对应 Agent 的 HealthCheckChannel 任务队列
    Log::Info("Hotmethod::Collect called for task_id=" + request->task_id());
    return grpc::Status::OK;
}

grpc::Status HotmethodServiceImpl::NotifyResult(grpc::ServerContext* /*context*/,
                                                  const hotmethod::TaskResult* request,
                                                  google::protobuf::Empty* /*response*/) {
    const std::string& tid = request->task_id();

    if (request->error_message().empty()) {
        Log::Info("NotifyResult: task=" + tid + " SUCCESS cos_key=" + request->cos_key());
    } else {
        Log::Warn("NotifyResult: task=" + tid + " FAILED err=" + request->error_message());
    }

    // 把结果写入内存表，ControlService::FetchData 会来取
    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_[tid] = *request; // 直接深拷贝整个 TaskResult（含 cos_key / error 等字段）
    }

    return grpc::Status::OK;
}

bool HotmethodServiceImpl::GetResult(const std::string& task_id,
                                      hotmethod::TaskResult* out) {
    std::lock_guard<std::mutex> lock(results_mutex_);
    auto it = results_.find(task_id);
    if (it == results_.end()) {
        return false;
    }
    *out = it->second;
    return true;
}

} // namespace dropd
