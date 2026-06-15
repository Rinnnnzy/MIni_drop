package server

// 任务状态（hotmethod_task.status）
// 每次状态迁移必须同时更新 status_info 字段，说明迁移原因。
const (
	TaskStatusPending   = 0 // 新建，尚未下发给 Agent
	TaskStatusRunning   = 1 // 已下发，Agent 正在采集
	TaskStatusDone      = 2 // 采集成功，文件已上传存储
	TaskStatusFailed    = 3 // 采集失败或超时
)

// 分析状态（hotmethod_task.analysis_status）
const (
	AnalysisStatusPending   = 0 // 等待分析
	AnalysisStatusRunning   = 1 // Analyzer 正在运行
	AnalysisStatusDone      = 2 // 分析完成，火焰图已生成
	AnalysisStatusFailed    = 3 // 分析失败
)

// HTTP 响应 code 字段
const (
	CodeOK          = 0       // 成功
	CodeBadRequest  = 4000001 // 请求参数错误
	CodeUnauth      = 4010001 // 未鉴权，前端收到后跳转登录页
	CodeNotFound    = 4040001 // 资源不存在
	CodeServerError = 5000001 // 服务内部错误
)

// 任务类型（TaskDesc.task_type）
const (
	TaskTypeCPU     = 0 // 通用 CPU 采样（perf）
	TaskTypeJava    = 1 // Java 应用采样（async-profiler）
	TaskTypeTracing = 2 // 全链路追踪
	TaskTypeMemory  = 4 // 内存采样
	TaskTypeJavaHeap = 6 // Java 堆 dump
)

// 采集器类型（TaskDesc.profiler_type）
const (
	ProfilerPerf          = 0 // Linux perf
	ProfilerAsyncProfiler = 1 // Java async-profiler
	ProfilerPprof         = 2 // Go/C++ pprof
)
