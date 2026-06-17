#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "hotmethod.grpc.pb.h"

#include "HealthCheckChannel.h"

namespace dropd {

// HotmethodChannel 是 Agent 的工作线程，负责"执行任务"这一侧。
//
// 与 HealthCheckChannel 的分工：
//   HealthCheckChannel（心跳线程）：和 Server 保持通信，把接收到的任务放进队列
//   HotmethodChannel（工作线程）：从队列里取任务，执行 perf → 上传 → 回报
//
// 这个两线程分工保证了：即使 perf 跑 30 秒，心跳仍然每秒都发出去，
// Server 不会因为心跳缺失误判 Agent 离线。
class HotmethodChannel {
public:
    // server_addr:  drop_server 的 gRPC 地址（用于调用 NotifyResult）
    // hc:           HealthCheckChannel 的指针（借用，不拥有），用于 PopTask
    // minio_ep:     MinIO 地址，例如 "minio:9000"
    // minio_ak/sk:  MinIO 访问凭证
    // minio_bucket: MinIO 桶名
    // minio_ssl:    是否使用 HTTPS
    HotmethodChannel(const std::string& server_addr,
                     HealthCheckChannel* hc,
                     const std::string& minio_ep,
                     const std::string& minio_ak,
                     const std::string& minio_sk,
                     const std::string& minio_bucket,
                     bool minio_ssl);
    ~HotmethodChannel();

    // Start 启动工作线程，必须在 hc.Start() 之后调用
    void Start();

    // Stop 停止工作线程；HealthCheckChannel 的 Stop 会 notify_all 唤醒 PopTask，
    // 这里只需要等待工作线程自然退出即可
    void Stop();

private:
    void Run();                                        // 工作循环主体
    void ExecTask(const hotmethod::TaskDesc& task);    // 执行单个任务

    std::string server_addr_;
    HealthCheckChannel* hc_; // 借用指针，生命周期由 agent/main.cpp 管理

    // MinIO 配置（从 Agent 自己的 config.json 读，不依赖 TaskDesc 里的 cos_config）
    std::string minio_ep_;
    std::string minio_ak_;
    std::string minio_sk_;
    std::string minio_bucket_;
    bool        minio_ssl_;

    // NotifyResult RPC 用的 stub，连接到 drop_server 的 Hotmethod 服务
    std::unique_ptr<hotmethod::Hotmethod::Stub> stub_;

    std::thread       worker_;
    std::atomic<bool> running_{false};
};

} // namespace dropd
