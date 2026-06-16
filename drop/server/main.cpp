#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "ControlService.h"
#include "HealthCheckService.h"
#include "InitAgentInfoService.h"
#include "Log.h"

int main(int argc, char** argv) {
  std::string listen_addr = "0.0.0.0:50051";
  if (argc > 1) {
    listen_addr = argv[1];
  }

  dropd::Log::Init("info");
  dropd::Log::Info("drop_server starting, listen=" + listen_addr);

  // ControlServiceImpl 必须先于 HealthCheckServiceImpl 创建，
  // 因为 HealthCheckServiceImpl 的构造函数需要它的指针来取任务队列。
  dropd::ControlServiceImpl control_service;
  dropd::HealthCheckServiceImpl health_service(&control_service);
  dropd::InitAgentInfoServiceImpl init_service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(listen_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&control_service);
  builder.RegisterService(&health_service);
  builder.RegisterService(&init_service);
  // HotmethodService（NotifyResult 完整实现）留待 Day 4 注册

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  if (!server) {
    dropd::Log::Error("failed to start grpc server on " + listen_addr);
    return 1;
  }

  dropd::Log::Info("drop_server listening on " + listen_addr);
  server->Wait(); // 阻塞直到收到关闭信号（gRPC 默认处理 SIGINT/SIGTERM 触发优雅关闭）

  return 0;
}
