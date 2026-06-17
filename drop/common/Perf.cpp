#include "Perf.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "Log.h"

namespace dropd {

PerfResult Perf::Record(int32_t pid,
                         uint32_t hz,
                         uint64_t duration_sec,
                         uint32_t timeout_sec,
                         const std::string& callgraph,
                         const std::string& output_path) {
    // 参数默认值处理
    if (hz == 0)           hz = 99;
    if (duration_sec == 0) duration_sec = 30;
    if (timeout_sec == 0)  timeout_sec = static_cast<uint32_t>(duration_sec) + 10;

    const std::string cg      = callgraph.empty() ? "dwarf" : callgraph;
    const std::string pid_str = std::to_string(pid);
    const std::string hz_str  = std::to_string(hz);
    const std::string dur_str = std::to_string(duration_sec);

    // 命令：perf record -F {hz} -p {pid} --call-graph {cg} -o {output} -- sleep {dur}
    // "-- sleep {dur}" 让 perf 采集恰好 duration_sec 秒后自然退出，无需额外的 SIGINT。
    std::vector<const char*> argv = {
        "perf", "record",
        "-F", hz_str.c_str(),
        "-p", pid_str.c_str(),
        "--call-graph", cg.c_str(),
        "-o", output_path.c_str(),
        "--", "sleep", dur_str.c_str(),
        nullptr
    };

    Log::Info("perf::Record start: pid=" + pid_str + " hz=" + hz_str
              + " dur=" + dur_str + " cg=" + cg + " out=" + output_path);

    // fork 子进程
    pid_t child = fork();
    if (child < 0) {
        return {false, std::string("fork failed: ") + strerror(errno)};
    }

    if (child == 0) {
        // ── 子进程 ──────────────────────────────────────────────
        // 放进独立进程组，这样 killpg 能把 perf 和它启动的 sleep 一起杀掉。
        // 子进程里调 setpgid(0,0)，父进程里也调（防止两者之间出现竞态）。
        setpgid(0, 0);
        execvp("perf", const_cast<char* const*>(argv.data()));
        // exec 失败才会执行到这里
        _exit(127); // 用 _exit 避免刷 atexit 钩子
    }

    // ── 父进程 ──────────────────────────────────────────────────
    // 消除父子之间 setpgid 的竞态：父进程也主动设置子进程的进程组
    setpgid(child, child);

    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::seconds(timeout_sec);

    while (true) {
        int wstatus = 0;
        pid_t ret = waitpid(child, &wstatus, WNOHANG);

        if (ret > 0) {
            // 子进程已经自然结束
            if (!WIFEXITED(wstatus)) {
                return {false, "perf terminated by signal " +
                               std::to_string(WTERMSIG(wstatus))};
            }
            int code = WEXITSTATUS(wstatus);
            if (code == 0) {
                Log::Info("perf::Record done: " + output_path);
                return {true, ""};
            }
            if (code == 127) {
                return {false, "perf not found; check PATH or install linux-tools"};
            }
            return {false, "perf exited with code " + std::to_string(code)};
        }

        if (ret < 0) {
            return {false, std::string("waitpid error: ") + strerror(errno)};
        }

        // ret == 0 → 子进程仍在运行，检查是否超时
        if (std::chrono::steady_clock::now() >= deadline) {
            Log::Warn("perf::Record timeout after " + std::to_string(timeout_sec)
                      + "s, killing pgid=" + std::to_string(child));
            killpg(child, SIGKILL);
            waitpid(child, nullptr, 0); // 必须回收，否则成为僵尸进程
            return {false, "perf record timed out after " +
                           std::to_string(timeout_sec) + "s"};
        }

        // 轮询间隔 200ms：粒度足够细，又不会空转消耗 CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

} // namespace dropd
