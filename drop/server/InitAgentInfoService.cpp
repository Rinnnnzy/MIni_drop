#include "InitAgentInfoService.h"

#include "Log.h"

namespace dropd {

grpc::Status InitAgentInfoServiceImpl::RegisterAgent(
    grpc::ServerContext* context,
    const init_svc::RegisterAgentRequest* request,
    init_svc::RegisterAgentResponse* response) {
  Log::Info("agent registered: uid=" + request->uid() +
            " host=" + request->host_name() +
            " ip=" + request->ip_addr() +
            " version=" + request->agent_version());

  // TODO Day 4: 把注册信息同步给 apiserver 的 agent_info 表
  // （目前只在 drop_server 自己的日志里记录，apiserver 的 Agent 列表还看不到它）
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status InitAgentInfoServiceImpl::FetchConfig(
    grpc::ServerContext* context,
    const init_svc::FetchConfigRequest* request,
    init_svc::FetchConfigResponse* response) {
  // Day 3 占位实现：直接返回和 docker-compose.yml 里 MinIO 配置一致的硬编码凭证。
  // Day 4 改为从 drop_server 自己的配置文件读取，而不是硬编码在代码里。
  drop::CosConfig* cos = response->mutable_cos_config();
  cos->set_endpoint("minio:9000");
  cos->set_bucket("drop");
  cos->set_access_key("drop");
  cos->set_secret_key("dropdrop");
  cos->set_use_ssl(false);

  response->set_success(true);
  return grpc::Status::OK;
}

} // namespace dropd
