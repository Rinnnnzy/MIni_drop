package server

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"

	"minidrop/apiserver/model"
	"minidrop/apiserver/util"
)

// ListAgents GET /api/v1/agents
// 返回当前用户可见的 Agent 列表（含在线状态）。
// Agent 在线/离线状态由 drop_server 维护，apiserver 从 agent_info 表读取。
func (s *APIServer) ListAgents(c *gin.Context) {
	uid := c.GetString("uid")

	var agents []model.AgentInfo
	if err := s.DB.Where("uid = ?", uid).Find(&agents).Error; err != nil {
		util.Logger.Error("ListAgents: db error", zap.Error(err))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}

	ok(c, gin.H{"agents": agents})
}

// StatAgent GET /api/v1/agent/stat?ip=x.x.x.x
// 查询某个 Agent 当前的实时资源占用情况。
// Day 2 先返回数据库里的静态信息，Day 4 接通 gRPC 后改为透传 drop_server.StatAgent()。
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

	// TODO Day 4: 改为调 gRPC drop_server.StatAgent() 获取实时资源数据
	ok(c, gin.H{
		"online":         agent.Online,
		"hostname":       agent.Hostname,
		"ip_addr":        agent.IPAddr,
		"version":        agent.Version,
		"last_seen":      agent.LastSeen,
		"cpu_percent":    0.0, // 占位，Day 4 填真实数据
		"rss_kb":         0,
	})
}
