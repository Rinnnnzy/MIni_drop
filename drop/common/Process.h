#pragma once
#include <cstdint>

namespace dropd {

// ProcessStats 保存从 /proc 读到的一次进程快照。
// 对应 common.proto 里的 PidStats 消息（字段名一一对应）。
struct ProcessStats {
    int32_t  pid         = 0;
    uint64_t rss_kb      = 0;     // 常驻内存 RSS，来自 /proc/{pid}/status VmRSS
    float    cpu_percent = 0.0f;  // 与上次调用的 CPU 占比，首次调用永远是 0
    uint64_t read_kb     = 0;     // 与上次调用之间的磁盘读取量（KB），来自 /proc/{pid}/io
    uint64_t write_kb    = 0;     // 与上次调用之间的磁盘写入量（KB），来自 /proc/{pid}/io
};

// Process 是读取 /proc 伪文件系统的静态工具类，无需实例化。
// 典型用途：心跳时把 Agent 自身和子进程的资源占用填进 self_pstats / children_pstats 上报。
class Process {
public:
    // GetStats 读取指定进程的当前资源快照。
    // 注意：cpu_percent 依赖两次采样差值，只有第二次及以后的调用才有非零值。
    static ProcessStats GetStats(int32_t pid);

    // GetSelfStats 读取当前进程自身的资源快照（等价于 GetStats(getpid())）。
    static ProcessStats GetSelfStats();
};

} // namespace dropd
