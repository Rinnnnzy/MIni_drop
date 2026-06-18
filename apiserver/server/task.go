package server

import (
	"context"
	"encoding/json"
	"net/http"
	"os"
	"os/exec"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"go.uber.org/zap"

	controlpb "minidrop/apiserver/proto/control"
	hotmethodpb "minidrop/apiserver/proto/hotmethod"

	"minidrop/apiserver/config"
	"minidrop/apiserver/model"
	"minidrop/apiserver/util"
)

// CreateTaskReq 是创建任务的请求体。
type CreateTaskReq struct {
	Name         string `json:"name"`
	Type         int    `json:"type"`          // 任务类型，见 const_enum.go
	ProfilerType int    `json:"profiler_type"` // 采集器类型
	TargetIP     string `json:"target_ip" binding:"required"`
	PID          int32  `json:"pid"  binding:"required"`
	Hz           uint32 `json:"hz"`        // 采样频率，默认 99
	Duration     uint64 `json:"duration"`  // 采集时长（秒），默认 30
	Callgraph    string `json:"callgraph"` // fp / dwarf / lbr
}

// CreateTask POST /api/v1/tasks
// 1. 在 DB 写一条 PENDING 任务
// 2. 调 drop_server gRPC CreateTask 下发给 Agent
// 3. 下发成功后把状态改为 RUNNING
func (s *APIServer) CreateTask(c *gin.Context) {
	var req CreateTaskReq
	if err := c.ShouldBindJSON(&req); err != nil {
		fail(c, http.StatusBadRequest, CodeBadRequest, "invalid params: "+err.Error())
		return
	}

	if req.Hz == 0 {
		req.Hz = 99
	}
	if req.Duration == 0 {
		req.Duration = 30
	}

	uid := c.GetString("uid")
	tid := uuid.New().String()[:8]

	paramsJSON, _ := json.Marshal(req)

	task := &model.HotmethodTask{
		TID:           tid,
		Name:          req.Name,
		Type:          req.Type,
		ProfilerType:  req.ProfilerType,
		TargetIP:      req.TargetIP,
		RequestParams: string(paramsJSON),
		Status:        TaskStatusPending,
		StatusInfo:    "task created, waiting to dispatch",
		UID:           uid,
		UserName:      c.GetString("user_name"),
	}

	if err := s.DB.Create(task).Error; err != nil {
		util.Logger.Error("CreateTask: db error", zap.Error(err))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}

	util.Logger.Info("task created", zap.String("tid", tid), zap.String("uid", uid))

	// ── 下发给 drop_server ──────────────────────────────────────────────
	if s.DropClient == nil {
		// drop_server 未配置（本地调试模式），只写 DB 不下发
		util.Logger.Warn("CreateTask: DropClient is nil, task stays PENDING", zap.String("tid", tid))
		ok(c, gin.H{"tid": tid})
		return
	}

	// 构建 TaskDesc（drop_server 会把它放进对应 Agent 的任务队列）
	timeoutSec := uint32(req.Duration) + 10 // 硬超时：采集时长 + 10s 余量
	taskDesc := &hotmethodpb.TaskDesc{
		TaskId:       tid,
		TaskType:     uint32(req.Type),
		ProfilerType: uint32(req.ProfilerType),
		SampleArgv: &hotmethodpb.RecordArgv{
			Hz:        req.Hz,
			Duration:  req.Duration,
			Pid:       req.PID,
			Callgraph: req.Callgraph,
		},
		TimeoutSec: timeoutSec,
		// cos_config 留空：drop_server（InitAgentInfoService）把 MinIO 凭证下发给 Agent，
		// Agent 用自己 config.json 里的 minio 配置上传，不依赖 TaskDesc 里的字段
	}

	grpcReq := &controlpb.CreateTaskRequest{
		TargetIp: req.TargetIP,
		Service:  "hotmethod",
		TaskDesc: taskDesc,
	}

	// gRPC 超时：独立 context，不受 HTTP 请求 context 影响
	timeoutDur := time.Duration(config.Global.GRPC.TimeoutSec) * time.Second
	if timeoutDur <= 0 {
		timeoutDur = 5 * time.Second
	}
	ctx, cancel := context.WithTimeout(context.Background(), timeoutDur)
	defer cancel()

	grpcResp, err := s.DropClient.CreateTask(ctx, grpcReq)
	if err != nil {
		util.Logger.Error("CreateTask: gRPC dispatch failed",
			zap.String("tid", tid), zap.Error(err))
		// 下发失败：把任务标记为 FAILED
		s.DB.Model(&model.HotmethodTask{}).Where("tid = ?", tid).
			Updates(map[string]interface{}{
				"status":      TaskStatusFailed,
				"status_info": "dispatch to drop_server failed: " + err.Error(),
			})
		fail(c, http.StatusBadGateway, CodeServerError, "dispatch to drop_server failed")
		return
	}
	if !grpcResp.GetSuccess() {
		msg := grpcResp.GetMessage()
		util.Logger.Error("CreateTask: drop_server rejected task",
			zap.String("tid", tid), zap.String("msg", msg))
		s.DB.Model(&model.HotmethodTask{}).Where("tid = ?", tid).
			Updates(map[string]interface{}{
				"status":      TaskStatusFailed,
				"status_info": "drop_server rejected: " + msg,
			})
		fail(c, http.StatusBadGateway, CodeServerError, "drop_server rejected: "+msg)
		return
	}

	// 下发成功：状态改为 RUNNING
	s.DB.Model(&model.HotmethodTask{}).Where("tid = ?", tid).
		Updates(map[string]interface{}{
			"status":      TaskStatusRunning,
			"status_info": "dispatched to agent at " + req.TargetIP,
		})

	util.Logger.Info("task dispatched",
		zap.String("tid", tid), zap.String("target_ip", req.TargetIP))

	// Background goroutine: poll FetchData until perf.data is uploaded, then trigger analysis.
	s.startCollectionWatcher(tid, req.Type, req.Duration)

	ok(c, gin.H{"tid": tid})
}

// ListTasks GET /api/v1/tasks?page=1&page_size=20
func (s *APIServer) ListTasks(c *gin.Context) {
	uid := c.GetString("uid")

	var tasks []model.HotmethodTask
	query := s.DB.Where("uid = ? AND deleted_at IS NULL", uid).
		Order("create_time DESC")

	page := 1
	pageSize := 20
	offset := (page - 1) * pageSize
	query = query.Offset(offset).Limit(pageSize)

	if err := query.Find(&tasks).Error; err != nil {
		util.Logger.Error("ListTasks: db error", zap.Error(err))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}

	ok(c, gin.H{"tasks": tasks, "total": len(tasks)})
}

// GetTask GET /api/v1/tasks/:tid
func (s *APIServer) GetTask(c *gin.Context) {
	tid := c.Param("tid")
	uid := c.GetString("uid")

	var task model.HotmethodTask
	if err := s.DB.Where("tid = ? AND uid = ? AND deleted_at IS NULL", tid, uid).
		First(&task).Error; err != nil {
		fail(c, http.StatusNotFound, CodeNotFound, "task not found")
		return
	}

	ok(c, task)
}

// DeleteTask DELETE /api/v1/tasks/:tid  软删除
func (s *APIServer) DeleteTask(c *gin.Context) {
	tid := c.Param("tid")
	uid := c.GetString("uid")

	result := s.DB.Where("tid = ? AND uid = ?", tid, uid).
		Delete(&model.HotmethodTask{})
	if result.Error != nil {
		util.Logger.Error("DeleteTask: db error", zap.Error(result.Error))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}
	if result.RowsAffected == 0 {
		fail(c, http.StatusNotFound, CodeNotFound, "task not found or no permission")
		return
	}

	ok(c, gin.H{"tid": tid})
}

// RetryTask POST /api/v1/tasks/:tid/retry
func (s *APIServer) RetryTask(c *gin.Context) {
	tid := c.Param("tid")
	uid := c.GetString("uid")

	var old model.HotmethodTask
	if err := s.DB.Where("tid = ? AND uid = ?", tid, uid).First(&old).Error; err != nil {
		fail(c, http.StatusNotFound, CodeNotFound, "original task not found")
		return
	}

	newTID := uuid.New().String()[:8]
	now := time.Now()
	newTask := model.HotmethodTask{
		TID:           newTID,
		Name:          old.Name + " (retry)",
		Type:          old.Type,
		ProfilerType:  old.ProfilerType,
		TargetIP:      old.TargetIP,
		RequestParams: old.RequestParams,
		Status:        TaskStatusPending,
		StatusInfo:    "retried from " + tid,
		UID:           uid,
		UserName:      old.UserName,
		CreateTime:    now,
	}

	if err := s.DB.Create(&newTask).Error; err != nil {
		util.Logger.Error("RetryTask: db error", zap.Error(err))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}

	ok(c, gin.H{"tid": newTID})
}

// startCollectionWatcher polls drop_server every 5 s until the agent uploads the
// perf.data file (or until timeout).  On success it triggers the Python analyzer.
func (s *APIServer) startCollectionWatcher(tid string, taskType int, durationSec uint64) {
	if s.DropClient == nil {
		return
	}
	go func() {
		timeout := time.Duration(durationSec+120) * time.Second
		deadline := time.Now().Add(timeout)
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()

		for {
			<-ticker.C
			if time.Now().After(deadline) {
				util.Logger.Warn("collection watcher timed out", zap.String("tid", tid))
				s.DB.Model(&model.HotmethodTask{}).Where("tid = ?", tid).
					Updates(map[string]interface{}{
						"status":      TaskStatusFailed,
						"status_info": "watcher timeout: no result within expected duration",
					})
				return
			}

			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			resp, err := s.DropClient.FetchData(ctx, &controlpb.FetchDataRequest{TaskId: tid})
			cancel()
			if err != nil {
				continue
			}

			if resp.GetSuccess() && resp.GetCosKey() != "" {
				now := time.Now()
				s.DB.Model(&model.HotmethodTask{}).Where("tid = ?", tid).
					Updates(map[string]interface{}{
						"status":          TaskStatusDone,
						"status_info":     "collection complete",
						"cos_key":         resp.GetCosKey(),
						"end_time":        now,
						"analysis_status": AnalysisStatusPending,
					})
				s.triggerAnalysis(tid, taskType)
				return
			}

			// Non-empty error that is NOT the "still running" sentinel → agent failure.
			if errMsg := resp.GetErrorMessage(); errMsg != "" && errMsg != "task not done yet or not found" {
				util.Logger.Error("agent reported collection failure",
					zap.String("tid", tid), zap.String("error", errMsg))
				s.DB.Model(&model.HotmethodTask{}).Where("tid = ?", tid).
					Updates(map[string]interface{}{
						"status":      TaskStatusFailed,
						"status_info": "agent failure: " + errMsg,
					})
				return
			}
		}
	}()
}

// triggerAnalysis marks analysis as RUNNING, then launches the Python analyzer as
// a subprocess.  The script itself writes analysis_status=DONE/FAILED when it finishes.
func (s *APIServer) triggerAnalysis(tid string, taskType int) {
	s.DB.Model(&model.HotmethodTask{}).Where("tid = ?", tid).
		Updates(map[string]interface{}{
			"analysis_status": AnalysisStatusRunning,
		})

	python := os.Getenv("ANALYSIS_PYTHON")
	if python == "" {
		python = "python3"
	}
	script := os.Getenv("ANALYSIS_SCRIPT")
	if script == "" {
		script = "/app/hotmethod_analyzer.py"
	}

	go func() {
		cmd := exec.Command(python, script,
			"--task-id", tid,
			"--task-type", strconv.Itoa(taskType),
		)
		cmd.Env = os.Environ()

		out, err := cmd.CombinedOutput()
		if err != nil {
			util.Logger.Error("analysis script failed",
				zap.String("tid", tid),
				zap.String("output", string(out)),
				zap.Error(err))
			s.DB.Model(&model.HotmethodTask{}).Where("tid = ?", tid).
				Updates(map[string]interface{}{
					"analysis_status": AnalysisStatusFailed,
				})
			return
		}
		util.Logger.Info("analysis script finished", zap.String("tid", tid))
	}()
}

// ListCosFiles GET /api/v1/cosfiles?tid=xxx
func (s *APIServer) ListCosFiles(c *gin.Context) {
	tid := c.Query("tid")
	if tid == "" {
		fail(c, http.StatusBadRequest, CodeBadRequest, "tid is required")
		return
	}

	prefix := tid + "/"
	keys, err := s.Storage.ListObjects(prefix)
	if err != nil {
		util.Logger.Error("ListCosFiles: list objects error", zap.Error(err))
		fail(c, http.StatusInternalServerError, CodeServerError, "storage error")
		return
	}

	type FileItem struct {
		Filename string `json:"filename"`
		URL      string `json:"url"`
	}

	var files []FileItem
	for _, key := range keys {
		signedURL, err := s.Storage.PreSign(key, 3600)
		if err != nil {
			util.Logger.Warn("ListCosFiles: presign failed",
				zap.String("key", key), zap.Error(err))
			continue
		}
		files = append(files, FileItem{Filename: key, URL: signedURL})
	}

	ok(c, gin.H{"files": files})
}
