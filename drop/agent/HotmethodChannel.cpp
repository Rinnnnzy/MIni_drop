#include "HotmethodChannel.h"

#include <sys/stat.h>  // mkdir
#include <sys/types.h>

#include <chrono>
#include <string>

#include "COSClient.h"
#include "Log.h"
#include "Perf.h"
#include "Process.h"
#include "hotmethod.pb.h"

namespace dropd {

HotmethodChannel::HotmethodChannel(const std::string& server_addr,
                                    HealthCheckChannel* hc,
                                    const std::string& minio_ep,
                                    const std::string& minio_ak,
                                    const std::string& minio_sk,
                                    const std::string& minio_bucket,
                                    bool minio_ssl)
    : server_addr_(server_addr),
      hc_(hc),
      minio_ep_(minio_ep),
      minio_ak_(minio_ak),
      minio_sk_(minio_sk),
      minio_bucket_(minio_bucket),
      minio_ssl_(minio_ssl) {
    auto channel = grpc::CreateChannel(server_addr_, grpc::InsecureChannelCredentials());
    stub_ = hotmethod::Hotmethod::NewStub(channel);
}

HotmethodChannel::~HotmethodChannel() {
    Stop();
}

void HotmethodChannel::Start() {
    running_ = true;

    // mc alias set：在工作线程启动前配置好，后续上传直接用
    COSClient cos(minio_ep_, minio_ak_, minio_sk_, minio_bucket_, minio_ssl_);
    std::string err;
    if (!cos.Init(&err)) {
        Log::Warn("HotmethodChannel: mc alias init failed: " + err
                  + " (upload will fail until mc is installed)");
        // 不阻塞启动，mc 不在就跳过，等实际执行任务时再失败
    }

    worker_ = std::thread(&HotmethodChannel::Run, this);
    Log::Info("worker thread started");
}

void HotmethodChannel::Stop() {
    // 只有第一次调用才真正执行 join
    // running_ 已由 HealthCheckChannel::Stop 里的 notify_all 触发 PopTask 返回 false
    if (running_.exchange(false) && worker_.joinable()) {
        worker_.join();
    }
}

void HotmethodChannel::Run() {
    Log::Info("HotmethodChannel::Run: waiting for tasks");

    while (running_) {
        hotmethod::TaskDesc task;

        // PopTask 阻塞等待：HealthCheckChannel 把任务塞进队列后唤醒这里
        // 返回 false 说明 HealthCheckChannel 已经 Stop()，工作线程跟着退出
        if (!hc_->PopTask(&task)) {
            break;
        }

        Log::Info("HotmethodChannel: got task=" + task.task_id());
        ExecTask(task);
    }

    Log::Info("HotmethodChannel::Run: exiting");
}

void HotmethodChannel::ExecTask(const hotmethod::TaskDesc& task) {
    const std::string& tid = task.task_id();

    // ── 1. 准备本地工作目录 ──────────────────────────────────────────────
    // /tmp/drop/{tid}/ 存放 perf.data 等中间产物
    const std::string work_dir  = "/tmp/drop/" + tid;
    const std::string perf_out  = work_dir + "/perf.data";

    // 递归创建目录（两层，先建父再建子）
    mkdir("/tmp/drop", 0755); // 已存在时返回 EEXIST，忽略
    if (mkdir(work_dir.c_str(), 0755) != 0) {
        Log::Warn("ExecTask: mkdir " + work_dir + " failed (may already exist)");
    }

    const auto& argv      = task.sample_argv();
    const uint32_t hz     = argv.hz()       > 0 ? argv.hz()       : 99;
    const uint64_t dur    = argv.duration() > 0 ? argv.duration() : 30;
    const uint32_t tmo    = task.timeout_sec() > 0 ? task.timeout_sec()
                                                    : static_cast<uint32_t>(dur) + 10;
    const int32_t  pid    = argv.pid();
    const std::string& cg = argv.callgraph();

    // ── 2. 采集 ──────────────────────────────────────────────────────────
    Log::Info("ExecTask: perf record start, tid=" + tid
              + " pid=" + std::to_string(pid)
              + " hz=" + std::to_string(hz)
              + " dur=" + std::to_string(dur) + "s");

    // 采集过程中每隔 1 秒记录一次自身资源（用于 self_pstats 上报）
    // Day 4 MVP：只在采集前后各采一个快照，不做完整时序记录
    ProcessStats pre_stat  = Process::GetSelfStats();
    PerfResult perf_result = Perf::Record(pid, hz, dur, tmo, cg, perf_out);
    ProcessStats post_stat = Process::GetSelfStats();

    // ── 3. 构建 TaskResult（无论成功失败都要上报） ──────────────────────
    hotmethod::TaskResult result;
    result.set_task_id(tid);

    // 把采集前后的自身资源快照填进 self_pstats
    auto* pre_ps  = result.add_self_pstats();
    pre_ps->set_pid(pre_stat.pid);
    pre_ps->set_rss_kb(pre_stat.rss_kb);
    pre_ps->set_cpu_percent(pre_stat.cpu_percent);
    pre_ps->set_read_kb(pre_stat.read_kb);
    pre_ps->set_write_kb(pre_stat.write_kb);

    auto* post_ps = result.add_self_pstats();
    post_ps->set_pid(post_stat.pid);
    post_ps->set_rss_kb(post_stat.rss_kb);
    post_ps->set_cpu_percent(post_stat.cpu_percent);
    post_ps->set_read_kb(post_stat.read_kb);
    post_ps->set_write_kb(post_stat.write_kb);

    if (!perf_result.success) {
        result.set_error_message("perf failed: " + perf_result.error_msg);
        Log::Error("ExecTask: " + result.error_message());
        // 即使失败也调 NotifyResult，让 Server 知道任务状态
    } else {
        // ── 4. 上传 perf.data 到 MinIO ──────────────────────────────────
        COSClient cos(minio_ep_, minio_ak_, minio_sk_, minio_bucket_, minio_ssl_);
        std::string init_err;
        if (!cos.Init(&init_err)) {
            result.set_error_message("cos init failed: " + init_err);
            Log::Error("ExecTask: " + result.error_message());
        } else {
            const std::string cos_key = tid + "/perf.data";
            std::string upload_err;
            if (!cos.PutFile(perf_out, cos_key, &upload_err)) {
                result.set_error_message("upload failed: " + upload_err);
                Log::Error("ExecTask: " + result.error_message());
            } else {
                result.set_cos_key(cos_key);
                Log::Info("ExecTask: upload done, cos_key=" + cos_key);
            }
        }
    }

    // ── 5. 调 NotifyResult 上报给 drop_server ────────────────────────────
    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));

    google::protobuf::Empty empty_resp;
    grpc::Status st = stub_->NotifyResult(&ctx, result, &empty_resp);
    if (!st.ok()) {
        Log::Error("ExecTask: NotifyResult RPC failed: " + st.error_message()
                   + " (task=" + tid + ")");
    } else {
        Log::Info("ExecTask: NotifyResult sent for task=" + tid);
    }
}

} // namespace dropd
