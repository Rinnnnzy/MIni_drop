#include "InitAgentInfoService.h"

#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Log.h"

namespace dropd {

// getEnv returns the value of an environment variable, or def if unset/empty.
// Used so that Docker deployments can override defaults via docker-compose env.
static std::string getEnv(const char* name, const char* def) {
  const char* val = ::getenv(name);
  return (val && *val) ? std::string(val) : std::string(def);
}

// NotifyApiserver POSTs agent registration info to apiserver.
// Uses APISERVER_URL env var (default http://localhost:8191) so the same binary
// works both on the host (localhost) and inside Docker (http://apiserver:8191).
static void NotifyApiserver(const std::string& hostname,
                             const std::string& ip_addr,
                             const std::string& version) {
  std::string base_url = getEnv("APISERVER_URL", "http://localhost:8191");
  std::string endpoint = base_url + "/internal/agent-register";

  std::string json =
      "{\"hostname\":\"" + hostname +
      "\",\"ip_addr\":\"" + ip_addr +
      "\",\"version\":\"" + version + "\"}";

  pid_t pid = ::fork();
  if (pid == 0) {
    ::execlp("curl", "curl",
             "-s", "-X", "POST",
             endpoint.c_str(),
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
  // MinIO connection info returned to the agent.
  // In Docker: set MINIO_ENDPOINT=minio:9000 via docker-compose environment.
  // On host:   defaults to localhost:9000.
  drop::CosConfig* cos = response->mutable_cos_config();
  cos->set_endpoint(getEnv("MINIO_ENDPOINT",   "localhost:9000"));
  cos->set_bucket(  getEnv("MINIO_BUCKET",     "drop"));
  cos->set_access_key(getEnv("MINIO_ACCESS_KEY", "drop"));
  cos->set_secret_key(getEnv("MINIO_SECRET_KEY", "dropdrop"));
  cos->set_use_ssl(false);

  response->set_success(true);
  return grpc::Status::OK;
}

} // namespace dropd
