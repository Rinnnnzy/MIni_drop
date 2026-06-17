#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "ControlService.h"
#include "HealthCheckService.h"
#include "HotmethodService.h"
#include "InitAgentInfoService.h"
#include "Log.h"

int main(int argc, char** argv) {
    std::string listen_addr = "0.0.0.0:50051";
    if (argc > 1) {
        listen_addr = argv[1];
    }

    dropd::Log::Init("info");
    dropd::Log::Info("drop_server starting, listen=" + listen_addr);

    // ── 服务实例的构造顺序有依赖关系，必须严格按下面的顺序 ──────────────────
    //
    // HotmethodService 先于 ControlService 创建：
    //   ControlService::FetchData 需要通过指针查询 HotmethodService 的结果表。
    //
    // ControlService 先于 HealthCheckService 创建：
    //   HealthCheckService 心跳时调用 ControlService::PopTaskForAgent 取任务。
    //
    // Day 3 的两层依赖 + Day 4 新增的 Hotmethod 层：
    //   HotmethodServiceImpl ← ControlServiceImpl ← HealthCheckServiceImpl

    dropd::HotmethodServiceImpl  hotmethod_service;                   // Day 4 新增
    dropd::ControlServiceImpl    control_service(&hotmethod_service); // 传入 hotmethod 指针
    dropd::HealthCheckServiceImpl health_service(&control_service);
    dropd::InitAgentInfoServiceImpl init_service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&hotmethod_service); // Day 4 新增：接收 Agent 的 NotifyResult
    builder.RegisterService(&control_service);
    builder.RegisterService(&health_service);
    builder.RegisterService(&init_service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        dropd::Log::Error("failed to start grpc server on " + listen_addr);
        return 1;
    }

    dropd::Log::Info("drop_server listening on " + listen_addr
                     + " (4 services: hotmethod, control, healthcheck, init)");
    server->Wait();

    return 0;
}
