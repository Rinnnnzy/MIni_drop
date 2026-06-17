#include "COSClient.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "Log.h"

namespace dropd {

// mc alias 名称：固定前缀防止与用户已有 alias 冲突
const char COSClient::kAlias[] = "drop_minio";

COSClient::COSClient(const std::string& endpoint,
                     const std::string& access_key,
                     const std::string& secret_key,
                     const std::string& bucket,
                     bool use_ssl)
    : endpoint_(endpoint),
      access_key_(access_key),
      secret_key_(secret_key),
      bucket_(bucket),
      use_ssl_(use_ssl) {}

bool COSClient::Init(std::string* err_msg) {
    // 构造 mc alias set drop_minio http(s)://endpoint ak sk
    std::string scheme = use_ssl_ ? "https://" : "http://";
    std::string url    = scheme + endpoint_;

    // mc alias set {alias} {url} {ak} {sk} --api s3v4
    std::vector<std::string> args = {
        "mc", "alias", "set", kAlias, url, access_key_, secret_key_,
        "--api", "s3v4"
    };

    int ret = RunCommand(args);
    if (ret != 0) {
        *err_msg = "mc alias set failed with code " + std::to_string(ret)
                   + "; ensure mc is installed and reachable";
        Log::Error("COSClient::Init: " + *err_msg);
        return false;
    }

    Log::Info("COSClient::Init: mc alias set to " + url);
    return true;
}

bool COSClient::PutFile(const std::string& local_path,
                        const std::string& cos_key,
                        std::string* err_msg) {
    // 目标路径格式：{alias}/{bucket}/{cos_key}
    // 例如：drop_minio/drop/tid/perf.data
    std::string dest = std::string(kAlias) + "/" + bucket_ + "/" + cos_key;

    std::vector<std::string> args = {"mc", "cp", local_path, dest};

    Log::Info("COSClient::PutFile: " + local_path + " -> " + dest);

    int ret = RunCommand(args);
    if (ret != 0) {
        *err_msg = "mc cp failed with code " + std::to_string(ret)
                   + " (src=" + local_path + " dst=" + dest + ")";
        Log::Error("COSClient::PutFile: " + *err_msg);
        return false;
    }

    Log::Info("COSClient::PutFile: upload done, cos_key=" + cos_key);
    return true;
}

// ── RunCommand：fork+exec，同步等待，返回退出码 ────────────────────────────────
// args[0] 是可执行文件名（由 PATH 查找），不是绝对路径。
// 返回 -1 表示 fork 或 exec 失败，非 0 表示子进程本身失败。
int COSClient::RunCommand(const std::vector<std::string>& args) {
    if (args.empty()) return -1;

    // 把 vector<string> 转成 execvp 需要的 char* const* 形式
    std::vector<const char*> cargs;
    cargs.reserve(args.size() + 1);
    for (const auto& s : args) {
        cargs.push_back(s.c_str());
    }
    cargs.push_back(nullptr);

    pid_t child = fork();
    if (child < 0) {
        Log::Error("COSClient::RunCommand: fork failed: " + std::string(strerror(errno)));
        return -1;
    }

    if (child == 0) {
        // 子进程：直接执行，不需要独立进程组（mc 本身不产生子进程）
        execvp(cargs[0], const_cast<char* const*>(cargs.data()));
        // 只有 exec 失败才到这里
        _exit(127);
    }

    // 父进程：同步等待
    int wstatus = 0;
    waitpid(child, &wstatus, 0);

    if (!WIFEXITED(wstatus)) {
        return -1; // 被信号杀死
    }
    return WEXITSTATUS(wstatus);
}

} // namespace dropd
