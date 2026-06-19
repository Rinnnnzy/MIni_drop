#include "InitAgentInfoService.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Log.h"

namespace dropd {

// NotifyApiserver 用 curl 把 Agent 注册信息 POST 到 apiserver 的内部接口，
// 写入 agent_info 表，使前端 Agent 列表可见。
// 采用 fork+exec curl 与 COSClient 的上传方式保持一致，不引入额外依赖。
static void NotifyApiserver(const std::string& hostname,
                             const std::string& ip_addr,
                             const std::string& version) {
  std::string json =
      "{\"hostname\":\"" + hostname +
      "\",\"ip_addr\":\"" + ip_addr +
      "\",\"version\":\"" + version + "\"}";

  pid_t pid = ::fork();
  if (pid == 0) {
    ::execlp("curl", "curl",
             "-s", "-X", "POST",
             "http://localhost:8191/internal/agent-register",
             "-H", "Content-Type: application/json",
             "-d", json.c_str(),
             nullptr);
    ::_exit(1);
  }
  if (pid > 0) {
    ::waitpid(pid, nullptr, 0);
  }
}

grpc::Status InitAgentInfoServiceImpl::RegisterAgent(
    grpc::ServerContext* context,
    const init_svc::RegisterAgentRequest* request,
    init_svc::RegisterAgentResponse* response) {
  Log::Info("agent registered: uid=" + request->uid() +
            " host=" + request->host_name() +
            " ip=" + request->ip_addr() +
            " version=" + request->agent_version());

  NotifyApiserver(request->host_name(), request->ip_addr(), request->agent_version());

  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status InitAgentInfoServiceImpl::FetchConfig(
    grpc::ServerContext* context,
    const init_svc::FetchConfigRequest* request,
    init_svc::FetchConfigResponse* response) {
  drop::CosConfig* cos = response->mutable_cos_config();
  cos->set_endpoint("localhost:9000");
  cos->set_bucket("drop");
  cos->set_access_key("drop");
  cos->set_secret_key("dropdrop");
  cos->set_use_ssl(false);

  response->set_success(true);
  return grpc::Status::OK;
}

} // namespace dropd
