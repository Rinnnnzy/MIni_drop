#pragma once
#include <grpcpp/grpcpp.h>
#include "init.grpc.pb.h"

namespace dropd {

// InitAgentInfoServiceImpl 处理 Agent 启动时的注册和配置拉取。
// Day 3：实现注册日志打印 + 返回占位 CosConfig。
// Day 4：把注册信息真正同步给 apiserver，CosConfig 改为从 drop_server 自己的配置读取。
class InitAgentInfoServiceImpl final : public init_svc::InitAgentInfo::Service {
public:
  grpc::Status RegisterAgent(grpc::ServerContext* context,
                              const init_svc::RegisterAgentRequest* request,
                              init_svc::RegisterAgentResponse* response) override;

  grpc::Status FetchConfig(grpc::ServerContext* context,
                            const init_svc::FetchConfigRequest* request,
                            init_svc::FetchConfigResponse* response) override;
};

} // namespace dropd
