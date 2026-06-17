#pragma once
#include <mutex>
#include <string>
#include <unordered_map>

#include <grpcpp/grpcpp.h>
#include "hotmethod.grpc.pb.h"

// google::protobuf::Empty 的定义通过 hotmethod.grpc.pb.h 间接引入（
// hotmethod.proto 导入了 google/protobuf/empty.proto）
#include <google/protobuf/empty.pb.h>

namespace dropd {

// HotmethodServiceImpl 实现 hotmethod.proto 定义的 Hotmethod gRPC 服务。
//
// NotifyResult RPC：Agent 采集完成后主动调用，把任务结果（cos_key / 错误信息）
//   写入内部映射表 results_，供 ControlService::FetchData 查询。
//
// Collect RPC：Server 主动推送任务给 Agent（备用路径，心跳是主路径），
//   Day 4 暂为占位实现，TODO Day 5 完善。
class HotmethodServiceImpl final : public hotmethod::Hotmethod::Service {
public:
    // Collect 是 Server 主动向 Agent 推任务的 RPC（备用路径，主路径是心跳响应）
    grpc::Status Collect(grpc::ServerContext* context,
                         const hotmethod::Target* request,
                         google::protobuf::Empty* response) override;

    // NotifyResult 是 Agent 采集完成后主动上报结果的 RPC
    grpc::Status NotifyResult(grpc::ServerContext* context,
                              const hotmethod::TaskResult* request,
                              google::protobuf::Empty* response) override;

    // GetResult 供 ControlService::FetchData 调用，查询某 task_id 的采集结果。
    // 返回 true 表示找到（任务已完成），false 表示尚未完成或不存在。
    // 这不是 gRPC 方法，是普通成员函数，通过指针在同进程内调用。
    bool GetResult(const std::string& task_id, hotmethod::TaskResult* out);

private:
    std::mutex                                            results_mutex_;
    std::unordered_map<std::string, hotmethod::TaskResult> results_; // task_id → result
};

} // namespace dropd
