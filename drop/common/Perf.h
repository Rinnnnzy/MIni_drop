#pragma once
#include <cstdint>
#include <string>

namespace dropd {

// PerfResult 记录一次 perf record 执行的结果。
struct PerfResult {
    bool        success;
    std::string error_msg; // success=false 时填写原因
};

// Perf 封装 perf record 命令的执行。
//
// 实现要点（易踩坑处）：
//   fork+exec 启动子进程 → setpgid 放进独立进程组
//   → 超时用 killpg 杀整组（杀到 perf 启动的所有子孙进程）
//   → waitpid 回收僵尸（防止进程表泄漏）
class Perf {
public:
    // Record 执行 perf record，采集目标进程的 CPU 调用栈数据。
    //
    // pid:          目标进程 PID，-1 表示全系统采样（需要 root 或 perf_event_paranoid≤-1）
    // hz:           采样频率（Hz），例如 99
    // duration_sec: 采集时长（秒），0 时默认 30
    // timeout_sec:  硬超时（秒），超时后强制终止整个进程组；应大于 duration_sec
    //               传 0 时自动设为 duration_sec+10
    // callgraph:    调用栈模式："fp" / "dwarf" / "lbr"；空字符串时用 "dwarf"
    // output_path:  perf.data 写入路径，调用方需确保父目录已存在
    static PerfResult Record(int32_t pid,
                             uint32_t hz,
                             uint64_t duration_sec,
                             uint32_t timeout_sec,
                             const std::string& callgraph,
                             const std::string& output_path);
};

} // namespace dropd
