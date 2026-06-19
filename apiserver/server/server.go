package server

import (
	"github.com/gin-gonic/gin"
	"go.uber.org/zap"
	"google.golang.org/grpc"
	"gorm.io/gorm"

	controlpb "minidrop/apiserver/proto/control"

	"minidrop/apiserver/middleware"
	"minidrop/apiserver/pkg/storage"
	"minidrop/apiserver/util"
)

// APIServer 持有所有全局资源，所有 HTTP handler 都作为它的方法。
// 这样各 handler 可以直接访问数据库、存储、日志，无需全局变量。
type APIServer struct {
	DB         *gorm.DB        // PostgreSQL 连接
	Storage    storage.Storage // 对象存储（MinIO）
	DropConn   *grpc.ClientConn       // drop_server gRPC 连接（用于 Close）
	DropClient controlpb.ControlClient // drop_server gRPC 客户端，nil 表示未配置
}

// New 创建 APIServer 实例。
// conn / client 在 drop_server 地址未配置时可以传 nil，
// 此时 CreateTask 仍然写 DB，但不下发到 drop_server（仅用于本地开发调试）。
func New(db *gorm.DB, st storage.Storage,
	conn *grpc.ClientConn, client controlpb.ControlClient) *APIServer {
	return &APIServer{
		DB:         db,
		Storage:    st,
		DropConn:   conn,
		DropClient: client,
	}
}

// RegisterRoutes 把所有 HTTP 路由注册到 gin.Engine。
func (s *APIServer) RegisterRoutes(r *gin.Engine) {
	// 全局中间件：CORS 必须在最前面，否则预检请求会被鉴权中间件拦截
	r.Use(middleware.CORS())
	r.Use(middleware.AccessLog())

	// 无需登录的接口
	r.GET("/healthz", s.Healthz)
	r.GET("/auth/check", s.CheckAuth)

	// 需要登录的 API
	api := r.Group("/api/v1", CheckLogin())
	{
		// 用户
		api.GET("/users/me", s.GetMe)

		// 任务相关
		api.POST("/tasks", s.CreateTask)
		api.GET("/tasks", s.ListTasks)
		api.GET("/tasks/:tid", s.GetTask)
		api.DELETE("/tasks/:tid", s.DeleteTask)
		api.POST("/tasks/:tid/retry", s.RetryTask)
		api.GET("/tasks/:tid/suggestions", s.GetSuggestions)
		api.GET("/cosfiles", s.ListCosFiles)

		// Agent 相关
		api.GET("/agents", s.ListAgents)
		api.GET("/agent/stat", s.StatAgent)
	}
}

// Healthz 健康检查接口，docker-compose healthcheck 和运维监控都依赖它。
func (s *APIServer) Healthz(c *gin.Context) {
	sqlDB, err := s.DB.DB()
	if err != nil || sqlDB.Ping() != nil {
		util.Logger.Error("healthz: db ping failed", zap.Error(err))
		c.JSON(503, gin.H{"status": "db_unavailable"})
		return
	}
	c.JSON(200, gin.H{"status": "ok"})
}

// ok 是统一成功响应的快捷方法。
func ok(c *gin.Context, data interface{}) {
	c.JSON(200, gin.H{"code": CodeOK, "data": data})
}

// fail 是统一失败响应的快捷方法。
func fail(c *gin.Context, httpStatus, code int, msg string) {
	c.JSON(httpStatus, gin.H{"code": code, "msg": msg})
}
