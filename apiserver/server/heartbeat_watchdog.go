package server

import (
	"time"

	"go.uber.org/zap"

	"minidrop/apiserver/model"
	"minidrop/apiserver/util"
)

const (
	heartbeatOfflineThreshold = 30 * time.Second // Agent 超过此时长无心跳即判离线
	heartbeatScanInterval     = 10 * time.Second  // 扫描周期
)

// StartHeartbeatWatchdog 启动一个后台 goroutine，定期扫描 agent_info 表，
// 把 last_seen 超过 30s 的在线 Agent 标记为离线，并写一条审计日志。
// Agent 重新心跳/注册时（RegisterAgentInternal）会自动把状态改回在线。
func (s *APIServer) StartHeartbeatWatchdog() {
	go func() {
		ticker := time.NewTicker(heartbeatScanInterval)
		defer ticker.Stop()

		for range ticker.C {
			s.markStaleAgentsOffline()
		}
	}()
	util.Logger.Info("heartbeat watchdog started",
		zap.Duration("threshold", heartbeatOfflineThreshold),
		zap.Duration("interval", heartbeatScanInterval))
}

func (s *APIServer) markStaleAgentsOffline() {
	cutoff := time.Now().Add(-heartbeatOfflineThreshold)

	var staleAgents []model.AgentInfo
	if err := s.DB.Where("online = ? AND last_seen < ?", true, cutoff).Find(&staleAgents).Error; err != nil {
		util.Logger.Error("heartbeat watchdog: query failed", zap.Error(err))
		return
	}

	for _, agent := range staleAgents {
		s.DB.Model(&model.AgentInfo{}).Where("ip_addr = ?", agent.IPAddr).
			Update("online", false)

		s.DB.Create(&model.AgentAuditLog{
			IPAddr:   agent.IPAddr,
			Hostname: agent.Hostname,
			Event:    "offline",
			Detail:   "no heartbeat for over 30s",
		})

		util.Logger.Warn("agent marked offline (heartbeat timeout)",
			zap.String("ip", agent.IPAddr),
			zap.String("host", agent.Hostname),
			zap.Time("last_seen", agent.LastSeen))
	}
}
