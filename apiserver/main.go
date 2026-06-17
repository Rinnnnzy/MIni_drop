package main

import (
	"flag"
	"fmt"
	"log"

	"github.com/gin-gonic/gin"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	controlpb "minidrop/apiserver/proto/control"

	"minidrop/apiserver/config"
	"minidrop/apiserver/model"
	"minidrop/apiserver/server"
	"minidrop/apiserver/util"
)

func main() {
	cfgFile := flag.String("c", "apiserver.yaml", "path to config file")
	flag.Parse()

	// 1. 加载配置
	if err := config.Load(*cfgFile); err != nil {
		log.Fatalf("load config failed: %v", err)
	}

	// 2. 初始化结构化日志
	if err := util.InitLogger(config.Global.Log.Level); err != nil {
		log.Fatalf("init logger failed: %v", err)
	}
	defer util.Logger.Sync()

	// 3. 连接 PostgreSQL
	db, err := util.NewDB(config.Global.Database)
	if err != nil {
		log.Fatalf("connect db failed: %v", err)
	}

	// 4. AutoMigrate
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

	// 6. 建立到 drop_server 的 gRPC 连接（可选，地址为空时跳过）
	// grpc.WithTransportCredentials(insecure.NewCredentials()) 是推荐的非 TLS 写法，
	// 替代已弃用的 grpc.WithInsecure()。
	var dropConn   *grpc.ClientConn
	var dropClient controlpb.ControlClient
	if addr := config.Global.GRPC.DropServerAddr; addr != "" {
		dropConn, err = grpc.Dial(
			addr,
			grpc.WithTransportCredentials(insecure.NewCredentials()),
		)
		if err != nil {
			// grpc.Dial 默认是非阻塞的（不等到连接建立），这里的错误只有在地址格式错误时才会触发。
			// 真正的连接失败会在第一次 RPC 调用时体现。
			log.Fatalf("grpc.Dial drop_server failed: %v", err)
		}
		defer dropConn.Close()
		dropClient = controlpb.NewControlClient(dropConn)
		log.Printf("drop_server gRPC target: %s (lazy connect)", addr)
	} else {
		log.Printf("GRPC.drop_server_addr not set, task dispatch disabled")
	}

	// 7. 创建 APIServer 并注册路由
	gin.SetMode(config.Global.Server.Mode)
	r := gin.New()

	s := server.New(db, st, dropConn, dropClient)
	s.RegisterRoutes(r)

	// 8. 启动 HTTP 服务
	addr := fmt.Sprintf(":%d", config.Global.Server.Port)
	log.Printf("apiserver starting on %s", addr)
	if err := r.Run(addr); err != nil {
		log.Fatalf("server error: %v", err)
	}
}
