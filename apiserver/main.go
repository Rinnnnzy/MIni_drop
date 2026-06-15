package main

import (
	"flag"
	"fmt"
	"log"

	"github.com/gin-gonic/gin"

	"minidrop/apiserver/config"
	"minidrop/apiserver/model"
	"minidrop/apiserver/server"
	"minidrop/apiserver/util"
)

func main() {
	// 从命令行读取配置文件路径，默认 apiserver.yaml
	cfgFile := flag.String("c", "apiserver.yaml", "path to config file")
	flag.Parse()

	// 1. 加载配置
	if err := config.Load(*cfgFile); err != nil {
		log.Fatalf("load config failed: %v", err)
	}

	// 2. 初始化结构化日志（JSON 格式，输出到 stdout）
	if err := util.InitLogger(config.Global.Log.Level); err != nil {
		log.Fatalf("init logger failed: %v", err)
	}
	defer util.Logger.Sync()

	// 3. 连接 PostgreSQL
	db, err := util.NewDB(config.Global.Database)
	if err != nil {
		log.Fatalf("connect db failed: %v", err)
	}

	// 4. AutoMigrate：自动建表（表不存在时创建，字段有变化时追加列）
	if err := db.AutoMigrate(
		&model.UserInfo{},
		&model.AgentInfo{},
		&model.HotmethodTask{},
		&model.MultiTask{},
		&model.Group{},
		&model.GroupMember{},
		&model.AnalysisSuggestion{},
	); err != nil {
		log.Fatalf("auto migrate failed: %v", err)
	}

	// 5. 初始化对象存储（MinIO）
	st, err := util.NewStorage(config.Global.Storage)
	if err != nil {
		log.Fatalf("init storage failed: %v", err)
	}

	// 6. 创建 APIServer 并注册路由
	gin.SetMode(config.Global.Server.Mode)
	r := gin.New() // 不使用 gin.Default()，避免重复日志

	s := server.New(db, st)
	s.RegisterRoutes(r)

	// 7. 启动 HTTP 服务
	addr := fmt.Sprintf(":%d", config.Global.Server.Port)
	log.Printf("apiserver starting on %s", addr)
	if err := r.Run(addr); err != nil {
		log.Fatalf("server error: %v", err)
	}
}
