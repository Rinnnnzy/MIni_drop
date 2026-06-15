package model

import (
	"time"

	"gorm.io/gorm"
)

// UserInfo 用户表。
// uid 是业务主键（来自 cookie），key 是用户的 API token。
type UserInfo struct {
	UID       string    `gorm:"primaryKey;column:uid"`
	Name      string    `gorm:"column:name;not null"`
	Groups    string    `gorm:"column:groups;type:jsonb;default:'[]'"` // 所属组 ID 列表，JSON 数组
	Key       string    `gorm:"column:key"`                             // API token
	CreatedAt time.Time `gorm:"column:created_at;autoCreateTime"`
}

func (UserInfo) TableName() string { return "user_info" }

// AgentInfo Agent 注册信息表。
// Agent 启动时写入，心跳时更新 online 状态。
type AgentInfo struct {
	ID          uint      `gorm:"primaryKey;autoIncrement"`
	Hostname    string    `gorm:"column:hostname;not null"`
	IPAddr      string    `gorm:"column:ip_addr;not null;uniqueIndex"` // Agent 所在机器 IP（唯一）
	Online      bool      `gorm:"column:online;default:false"`
	UID         string    `gorm:"column:uid"`          // 归属用户
	GID         string    `gorm:"column:gid"`          // 归属组
	Version     string    `gorm:"column:version"`
	Environment string    `gorm:"column:environment"` // 运行环境描述
	LastSeen    time.Time `gorm:"column:last_seen"`   // 最后一次心跳时间
	CreatedAt   time.Time `gorm:"column:created_at;autoCreateTime"`
}

func (AgentInfo) TableName() string { return "agent_info" }

// HotmethodTask 任务核心表，记录每一个采集任务的完整生命周期。
// status 和 analysis_status 每次变更必须同时更新 status_info 字段（记录原因）。
type HotmethodTask struct {
	ID             uint           `gorm:"primaryKey;autoIncrement"`
	TID            string         `gorm:"column:tid;uniqueIndex;not null"` // 全局唯一任务 ID
	Name           string         `gorm:"column:name"`
	Type           int            `gorm:"column:type;default:0"`            // 任务类型，见 const_enum.go
	ProfilerType   int            `gorm:"column:profiler_type;default:0"`   // 采集器类型
	TargetIP       string         `gorm:"column:target_ip;not null"`        // 目标 Agent IP
	RequestParams  string         `gorm:"column:request_params;type:jsonb"` // 原始请求参数（JSON）
	Status         int            `gorm:"column:status;default:0"`          // 任务状态 0~3
	StatusInfo     string         `gorm:"column:status_info"`               // 状态变更原因（必填）
	AnalysisStatus int            `gorm:"column:analysis_status;default:0"` // 分析状态 0~3
	CosKey         string         `gorm:"column:cos_key"`                   // 采集结果在对象存储的路径
	UID            string         `gorm:"column:uid;index"`                 // 创建人
	UserName       string         `gorm:"column:user_name"`
	MasterTaskTID  string         `gorm:"column:master_task_tid"`           // 父任务 TID（批量任务用）
	CreateTime     time.Time      `gorm:"column:create_time;autoCreateTime"`
	BeginTime      *time.Time     `gorm:"column:begin_time"`  // Agent 开始采集时间
	EndTime        *time.Time     `gorm:"column:end_time"`    // 采集结束时间
	DeletedAt      gorm.DeletedAt `gorm:"index"`              // 软删除
}

func (HotmethodTask) TableName() string { return "hotmethod_task" }

// MultiTask 批量任务表，一个父任务包含多个子任务 TID。
type MultiTask struct {
	TID            string    `gorm:"primaryKey;column:tid"`
	SubTIDs        string    `gorm:"column:sub_tids;type:jsonb;default:'[]'"` // 子任务 TID 列表
	Type           int       `gorm:"column:type;default:0"`
	Status         int       `gorm:"column:status;default:0"`
	AnalysisStatus int       `gorm:"column:analysis_status;default:0"`
	TriggerType    int       `gorm:"column:trigger_type;default:0"` // 0=手动 / 1=定时
	CreatedAt      time.Time `gorm:"column:created_at;autoCreateTime"`
}

func (MultiTask) TableName() string { return "multi_tasks" }

// Group 用户组表。
type Group struct {
	GID       string    `gorm:"primaryKey;column:gid"`
	Name      string    `gorm:"column:name;not null"`
	OwnerID   string    `gorm:"column:owner_id"` // 组长 UID
	CreatedAt time.Time `gorm:"column:created_at;autoCreateTime"`
}

func (Group) TableName() string { return "groups" }

// GroupMember 组成员关系表（多对多）。
type GroupMember struct {
	GID string `gorm:"primaryKey;column:gid"`
	UID string `gorm:"primaryKey;column:uid"`
}

func (GroupMember) TableName() string { return "group_members" }

// AnalysisSuggestion 分析建议表，存放 Analyzer 产出的热点函数和优化建议。
type AnalysisSuggestion struct {
	ID           uint      `gorm:"primaryKey;autoIncrement"`
	TID          string    `gorm:"column:tid;index;not null"` // 关联的任务 TID
	FuncName     string    `gorm:"column:func"`               // 热点函数名
	Suggestion   string    `gorm:"column:suggestion;type:text"`    // 规则引擎建议
	AISuggestion string    `gorm:"column:ai_suggestion;type:text"` // LLM 建议（加分项）
	Status       int       `gorm:"column:status;default:0"`
	CreatedAt    time.Time `gorm:"column:created_at;autoCreateTime"`
}

func (AnalysisSuggestion) TableName() string { return "analysis_suggestion" }
