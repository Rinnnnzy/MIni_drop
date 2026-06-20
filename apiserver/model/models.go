package model

import (
	"time"

	"gorm.io/gorm"
)

type UserInfo struct {
	UID       string    `gorm:"primaryKey;column:uid"                       json:"uid"`
	Name      string    `gorm:"column:name;not null"                         json:"name"`
	Groups    string    `gorm:"column:groups;type:jsonb;default:'[]'"        json:"groups"`
	Key       string    `gorm:"column:key"                                   json:"key"`
	CreatedAt time.Time `gorm:"column:created_at;autoCreateTime"             json:"created_at"`
}

func (UserInfo) TableName() string { return "user_info" }

type AgentInfo struct {
	ID          uint      `gorm:"primaryKey;autoIncrement"                     json:"id"`
	Hostname    string    `gorm:"column:hostname;not null"                     json:"hostname"`
	IPAddr      string    `gorm:"column:ip_addr;not null;uniqueIndex"          json:"ip_addr"`
	Online      bool      `gorm:"column:online;default:false"                  json:"online"`
	UID         string    `gorm:"column:uid"                                   json:"uid"`
	GID         string    `gorm:"column:gid"                                   json:"gid"`
	Version     string    `gorm:"column:version"                               json:"version"`
	Environment string    `gorm:"column:environment"                           json:"environment"`
	LastSeen    time.Time `gorm:"column:last_seen"                             json:"last_seen"`
	CreatedAt   time.Time `gorm:"column:created_at;autoCreateTime"             json:"created_at"`
}

func (AgentInfo) TableName() string { return "agent_info" }

// AgentAuditLog 记录 Agent 上线/离线事件，供 Web 审计追溯。
type AgentAuditLog struct {
	ID        uint      `gorm:"primaryKey;autoIncrement"         json:"id"`
	IPAddr    string    `gorm:"column:ip_addr;not null;index"    json:"ip_addr"`
	Hostname  string    `gorm:"column:hostname"                  json:"hostname"`
	Event     string    `gorm:"column:event;not null"            json:"event"` // "online" / "offline"
	Detail    string    `gorm:"column:detail"                    json:"detail"`
	CreatedAt time.Time `gorm:"column:created_at;autoCreateTime" json:"created_at"`
}

func (AgentAuditLog) TableName() string { return "agent_audit_log" }

type HotmethodTask struct {
	ID             uint           `gorm:"primaryKey;autoIncrement"                 json:"id"`
	TID            string         `gorm:"column:tid;uniqueIndex;not null"           json:"tid"`
	Name           string         `gorm:"column:name"                               json:"name"`
	Type           int            `gorm:"column:type;default:0"                     json:"type"`
	ProfilerType   int            `gorm:"column:profiler_type;default:0"            json:"profiler_type"`
	TargetIP       string         `gorm:"column:target_ip;not null"                 json:"target_ip"`
	RequestParams  string         `gorm:"column:request_params;type:jsonb"          json:"request_params"`
	Status         int            `gorm:"column:status;default:0"                   json:"status"`
	StatusInfo     string         `gorm:"column:status_info"                        json:"status_info"`
	AnalysisStatus int            `gorm:"column:analysis_status;default:0"          json:"analysis_status"`
	CosKey         string         `gorm:"column:cos_key"                            json:"cos_key"`
	UID            string         `gorm:"column:uid;index"                          json:"uid"`
	UserName       string         `gorm:"column:user_name"                          json:"user_name"`
	MasterTaskTID  string         `gorm:"column:master_task_tid"                    json:"master_task_tid"`
	CreateTime     time.Time      `gorm:"column:create_time;autoCreateTime"         json:"create_time"`
	BeginTime      *time.Time     `gorm:"column:begin_time"                         json:"begin_time"`
	EndTime        *time.Time     `gorm:"column:end_time"                           json:"end_time"`
	DeletedAt      gorm.DeletedAt `gorm:"index"                                     json:"-"`
}

func (HotmethodTask) TableName() string { return "hotmethod_task" }

type MultiTask struct {
	TID            string    `gorm:"primaryKey;column:tid"                        json:"tid"`
	SubTIDs        string    `gorm:"column:sub_tids;type:jsonb;default:'[]'"      json:"sub_tids"`
	Type           int       `gorm:"column:type;default:0"                        json:"type"`
	Status         int       `gorm:"column:status;default:0"                      json:"status"`
	AnalysisStatus int       `gorm:"column:analysis_status;default:0"             json:"analysis_status"`
	TriggerType    int       `gorm:"column:trigger_type;default:0"                json:"trigger_type"`
	CreatedAt      time.Time `gorm:"column:created_at;autoCreateTime"             json:"created_at"`
}

func (MultiTask) TableName() string { return "multi_tasks" }

type Group struct {
	GID       string    `gorm:"primaryKey;column:gid"                        json:"gid"`
	Name      string    `gorm:"column:name;not null"                         json:"name"`
	OwnerID   string    `gorm:"column:owner_id"                              json:"owner_id"`
	CreatedAt time.Time `gorm:"column:created_at;autoCreateTime"             json:"created_at"`
}

func (Group) TableName() string { return "groups" }

type GroupMember struct {
	GID string `gorm:"primaryKey;column:gid" json:"gid"`
	UID string `gorm:"primaryKey;column:uid" json:"uid"`
}

func (GroupMember) TableName() string { return "group_members" }

type AnalysisSuggestion struct {
	ID           uint      `gorm:"primaryKey;autoIncrement"                     json:"id"`
	TID          string    `gorm:"column:tid;index;not null"                    json:"tid"`
	FuncName     string    `gorm:"column:func"                                  json:"func"`
	Suggestion   string    `gorm:"column:suggestion;type:text"                  json:"suggestion"`
	AISuggestion string    `gorm:"column:ai_suggestion;type:text"               json:"ai_suggestion"`
	Status       int       `gorm:"column:status;default:0"                      json:"status"`
	CreatedAt    time.Time `gorm:"column:created_at;autoCreateTime"             json:"created_at"`
}

func (AnalysisSuggestion) TableName() string { return "analysis_suggestion" }
