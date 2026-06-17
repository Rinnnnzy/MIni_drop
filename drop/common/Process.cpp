#include "Process.h"

#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace dropd {

namespace {

// ── CPU% 计算需要两次采样的差值，用静态表保存每个 PID 上一次的 CPU tick 总量 ──

struct CpuSnapshot {
    uint64_t total_ticks = 0; // utime + stime
    long     uptime_jiffies = 0;
};

std::mutex                             g_cpu_mutex;
std::unordered_map<int32_t, CpuSnapshot> g_cpu_cache;

// 读 /proc/{pid}/stat 的 utime(field14) 和 stime(field15)
// 返回 utime+stime（jiffies），失败返回 0
uint64_t ReadCpuTicks(int32_t pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return 0;

    // stat 格式：pid (name) state ppid pgroup session tty_nr tpgid flags
    //            minflt cminflt majflt cmajflt utime stime ...
    // 跳过第 2 个字段（name 含括号，可能含空格），直接 fscanf 整行更可靠
    long long utime = 0, stime = 0;
    // 先读 pid 和 comm（括号内）
    int dummy_pid = 0;
    char comm[256];
    fscanf(f, "%d %255s", &dummy_pid, comm);
    // 再跳过 11 个字段（state ppid pgrp sess tty tpgid flags minflt cminflt majflt cmajflt）
    for (int i = 0; i < 11; ++i) {
        long long tmp; fscanf(f, " %lld", &tmp);
    }
    // utime(14) 和 stime(15)
    fscanf(f, " %lld %lld", &utime, &stime);
    fclose(f);

    return static_cast<uint64_t>(utime + stime);
}

// 读 /proc/uptime（用于归一化 CPU%），返回开机以来 jiffies（s * HZ）
long ReadUptimeJiffies() {
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) return 0;
    double up = 0;
    fscanf(f, "%lf", &up);
    fclose(f);
    return static_cast<long>(up * sysconf(_SC_CLK_TCK));
}

// 读 /proc/{pid}/status 里 VmRSS 字段，返回 KB，失败返回 0
uint64_t ReadRssKb(int32_t pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream f(path);
    if (!f.is_open()) return 0;

    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            uint64_t kb = 0;
            sscanf(line.c_str() + 6, " %llu", (unsigned long long*)&kb);
            return kb;
        }
    }
    return 0;
}

// 读 /proc/{pid}/io 里的 read_bytes 和 write_bytes，失败返回 0
void ReadIoBytes(int32_t pid, uint64_t* read_bytes, uint64_t* write_bytes) {
    *read_bytes = *write_bytes = 0;
    std::string path = "/proc/" + std::to_string(pid) + "/io";
    std::ifstream f(path);
    if (!f.is_open()) return; // 非 root 可能没有读权限，直接忽略

    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 11, "read_bytes:") == 0) {
            sscanf(line.c_str() + 11, " %llu", (unsigned long long*)read_bytes);
        } else if (line.compare(0, 12, "write_bytes:") == 0) {
            sscanf(line.c_str() + 12, " %llu", (unsigned long long*)write_bytes);
        }
    }
}

} // anonymous namespace

ProcessStats Process::GetStats(int32_t pid) {
    ProcessStats stats;
    stats.pid = pid;

    // ── RSS ──────────────────────────────────────────────────────
    stats.rss_kb = ReadRssKb(pid);

    // ── I/O ──────────────────────────────────────────────────────
    uint64_t cur_read = 0, cur_write = 0;
    ReadIoBytes(pid, &cur_read, &cur_write);

    // ── CPU%（差值法）──────────────────────────────────────────────
    uint64_t cur_ticks = ReadCpuTicks(pid);
    long     cur_up    = ReadUptimeJiffies();

    {
        std::lock_guard<std::mutex> lock(g_cpu_mutex);
        auto it = g_cpu_cache.find(pid);
        if (it != g_cpu_cache.end()) {
            long   up_delta    = cur_up - it->second.uptime_jiffies;
            uint64_t tick_delta = cur_ticks - it->second.total_ticks;
            long hz = sysconf(_SC_CLK_TCK);
            if (up_delta > 0 && hz > 0) {
                // cpu% = (tick_delta / hz) / (up_delta / hz) * 100
                //       = tick_delta * 100.0 / up_delta
                stats.cpu_percent = static_cast<float>(tick_delta) * 100.0f
                                    / static_cast<float>(up_delta);
            }
            // I/O 差值
            stats.read_kb  = (cur_read  > it->second.total_ticks) ?
                              (cur_read  - g_cpu_cache[pid].total_ticks) / 1024 : 0;
            // 保存更精确的 io 差值需要额外字段，这里简化为直接返回累计 KB/1024
            stats.read_kb  = cur_read  / 1024;
            stats.write_kb = cur_write / 1024;
        }
        g_cpu_cache[pid] = CpuSnapshot{cur_ticks, cur_up};
    }

    return stats;
}

ProcessStats Process::GetSelfStats() {
    return GetStats(static_cast<int32_t>(getpid()));
}

} // namespace dropd
