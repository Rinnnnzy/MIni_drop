package server

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"

	"minidrop/apiserver/model"
	"minidrop/apiserver/util"
)

// ListAgents GET /api/v1/agents
// 返回所有已注册 Agent 列表（含在线状态）。
func (s *APIServer) ListAgents(c *gin.Context) {
	var agents []model.AgentInfo
	if err := s.DB.Find(&agents).Error; err != nil {
		util.Logger.Error("ListAgents: db error", zap.Error(err))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}
	ok(c, gin.H{"agents": agents})
}

// StatAgent GET /api/v1/agent/stat?ip=x.x.x.x
func (s *APIServer) StatAgent(c *gin.Context) {
	ip := c.Query("ip")
	if ip == "" {
		fail(c, http.StatusBadRequest, CodeBadRequest, "ip is required")
		return
	}

	var agent model.AgentInfo
	if err := s.DB.Where("ip_addr = ?", ip).First(&agent).Error; err != nil {
		fail(c, http.StatusNotFound, CodeNotFound, "agent not found")
		return
	}

	ok(c, gin.H{
		"online":      agent.Online,
		"hostname":    agent.Hostname,
		"ip_addr":     agent.IPAddr,
		"version":     agent.Version,
		"last_seen":   agent.LastSeen,
		"cpu_percent": 0.0,
		"rss_kb":      0,
	})
}

// ListAgentAuditLog GET /api/v1/agents/audit-log
// 返回 Agent 上线/离线审计记录，按时间倒序，最多 100 条。
func (s *APIServer) ListAgentAuditLog(c *gin.Context) {
	var logs []model.AgentAuditLog
	if err := s.DB.Order("created_at DESC").Limit(100).Find(&logs).Error; err != nil {
		util.Logger.Error("ListAgentAuditLog: db error", zap.Error(err))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}
	ok(c, gin.H{"logs": logs})
}

// RegisterAgentInternal POST /internal/agent-register
// drop_server 在 Agent 首次注册、以及之后每次收到心跳时都会调用，
// 用于把 Agent 信息和最新心跳时间同步到 agent_info 表。
// 不需要用户鉴权，只在内部网络使用。
func (s *APIServer) RegisterAgentInternal(c *gin.Context) {
	var req struct {
		Hostname string `json:"hostname"`
		IPAddr   string `json:"ip_addr" binding:"required"`
		Version  string `json:"version"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		fail(c, http.StatusBadRequest, CodeBadRequest, err.Error())
		return
	}

	now := time.Now()
	var agent model.AgentInfo
	result := s.DB.Where("ip_addr = ?", req.IPAddr).First(&agent)
	wasOffline := result.Error != nil || !agent.Online // 新建 或 之前离线，都算一次"上线"事件

	if result.Error != nil {
		// 首次注册：新建
		agent = model.AgentInfo{
			Hostname: req.Hostname,
			IPAddr:   req.IPAddr,
			Online:   true,
			Version:  req.Version,
			LastSeen: now,
		}
		if err := s.DB.Create(&agent).Error; err != nil {
			util.Logger.Error("RegisterAgentInternal: create failed", zap.Error(err))
			fail(c, http.StatusInternalServerError, CodeServerError, "db error")
			return
		}
	} else {
		// 重启后重新注册：更新
		s.DB.Model(&agent).Updates(map[string]interface{}{
			"hostname":  req.Hostname,
			"online":    true,
			"version":   req.Version,
			"last_seen": now,
		})
	}

	if wasOffline {
		s.DB.Create(&model.AgentAuditLog{
			IPAddr:   req.IPAddr,
			Hostname: req.Hostname,
			Event:    "online",
			Detail:   "agent registered/recovered",
		})
	}

	util.Logger.Info("agent registered",
		zap.String("ip", req.IPAddr),
		zap.String("host", req.Hostname))
	ok(c, gin.H{"ok": true})
}

// RegisterAgentOffline POST /internal/agent-offline
// drop_server 的心跳扫描线程检测到某 Agent 超过 30s 无心跳时调用，
// 把该 Agent 标记离线并写入审计日志，供 Web 端 Agent 列表展示。
// 不需要用户鉴权，只在内部网络使用。
func (s *APIServer) RegisterAgentOffline(c *gin.Context) {
	var req struct {
		Hostname string `json:"hostname"`
		IPAddr   string `json:"ip_addr" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		fail(c, http.StatusBadRequest, CodeBadRequest, err.Error())
		return
	}

	result := s.DB.Model(&model.AgentInfo{}).Where("ip_addr = ?", req.IPAddr).
		Update("online", false)
	if result.Error != nil {
		util.Logger.Error("RegisterAgentOffline: db error", zap.Error(result.Error))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}

	s.DB.Create(&model.AgentAuditLog{
		IPAddr:   req.IPAddr,
		Hostname: req.Hostname,
		Event:    "offline",
		Detail:   "no heartbeat for over 30s (reported by drop_server)",
	})

	util.Logger.Warn("agent marked offline", zap.String("ip", req.IPAddr))
	ok(c, gin.H{"ok": true})
}
