package config

import (
	"log"
	"strings"

	"github.com/spf13/viper"
)

// Config 是整个 apiserver 的配置结构体，对应 apiserver.yaml 的层级。
type Config struct {
	Server   ServerConfig
	Database DatabaseConfig
	GRPC     GRPCConfig
	Storage  StorageConfig
	Log      LogConfig
}

type ServerConfig struct {
	Port int    // HTTP 监听端口，默认 8191
	Mode string // gin 运行模式：debug / release
}

type DatabaseConfig struct {
	DSN          string // PostgreSQL 连接串
	MaxOpenConns int    `mapstructure:"max_open_conns"` // 最大开放连接数
	MaxIdleConns int    `mapstructure:"max_idle_conns"` // 最大空闲连接数
}

type GRPCConfig struct {
	DropServerAddr string `mapstructure:"drop_server_addr"` // drop_server 的 gRPC 地址
	TimeoutSec     int    `mapstructure:"timeout_sec"`      // gRPC 调用超时秒数
}

type StorageConfig struct {
	Endpoint        string // MinIO 地址，例如 minio:9000
	PresignEndpoint string `mapstructure:"presign_endpoint"` // 浏览器可访问的地址，例如 localhost:9000；空表示与 Endpoint 相同
	AccessKey       string `mapstructure:"access_key"`       // 访问密钥 ID
	SecretKey       string `mapstructure:"secret_key"`       // 访问密钥 Secret
	Bucket          string // 桶名，例如 drop
	UseSSL          bool   `mapstructure:"use_ssl"`          // 是否启用 HTTPS
	PresignTTLSec   int    `mapstructure:"presign_ttl_sec"`  // 预签名 URL 有效期（秒）
}

type LogConfig struct {
	Level string // 日志级别：debug / info / warn / error
}

// Global 是全局配置实例，加载后供所有包直接读取。
var Global Config

// Load 从 cfgFile 路径读取 YAML 配置并填充到 Global。
func Load(cfgFile string) error {
	viper.SetConfigFile(cfgFile)
	// 嵌套配置项（如 storage.presign_endpoint）对应的环境变量用下划线代替点
	// （例如 STORAGE_PRESIGN_ENDPOINT），否则 AutomaticEnv 默认不会匹配嵌套 key。
	viper.SetEnvKeyReplacer(strings.NewReplacer(".", "_"))
	viper.AutomaticEnv() // 允许用环境变量覆盖配置项

	if err := viper.ReadInConfig(); err != nil {
		return err
	}

	if err := viper.Unmarshal(&Global); err != nil {
		return err
	}

	log.Printf("[config] loaded from %s", cfgFile)
	return nil
}
