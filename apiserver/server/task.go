package server

import (
	"encoding/json"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"go.uber.org/zap"

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
	Hz           uint32 `json:"hz"`       // 采样频率，默认 99
	Duration     uint64 `json:"duration"` // 采集时长（秒），默认 30
	Callgraph    string `json:"callgraph"` // fp / dwarf / lbr
}

// CreateTask POST /api/v1/tasks
// 在数据库写一条 PENDING 任务，Day 4 才真正调 gRPC 下发给 drop_server。
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
	tid := uuid.New().String()[:8] // 短 UUID 作为任务 ID

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

	// TODO Day 4: 调 drop_server gRPC CreateTask，下发任务给 Agent
	// 下发成功后把 status 改为 TaskStatusRunning

	ok(c, gin.H{"tid": tid})
}

// ListTasks GET /api/v1/tasks?page=1&page_size=20
// 返回当前用户的任务列表，按创建时间倒序分页。
func (s *APIServer) ListTasks(c *gin.Context) {
	uid := c.GetString("uid")

	var tasks []model.HotmethodTask
	query := s.DB.Where("uid = ? AND deleted_at IS NULL", uid).
		Order("create_time DESC")

	// 简单分页
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
// 返回任务详情，包含 cos_key（采集结果路径）和 analysis_status。
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

// DeleteTask DELETE /api/v1/tasks/:tid  软删除（设置 deleted_at）
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
// 用原参数重新创建一个新任务。
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

// ListCosFiles GET /api/v1/cosfiles?tid=xxx
// 列出任务的所有产出文件，并为每个文件生成预签名临时 URL。
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
			util.Logger.Warn("ListCosFiles: presign failed", zap.String("key", key), zap.Error(err))
			continue
		}
		files = append(files, FileItem{Filename: key, URL: signedURL})
	}

	ok(c, gin.H{"files": files})
}
