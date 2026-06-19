package util

import (
	"minidrop/apiserver/config"
	"minidrop/apiserver/pkg/storage"
	minioimpl "minidrop/apiserver/pkg/storage/minio"
)

// NewStorage 根据配置初始化对象存储客户端。
// 目前只实现了 MinIO，返回 storage.Storage 接口供上层使用。
func NewStorage(cfg config.StorageConfig) (storage.Storage, error) {
	return minioimpl.New(
		cfg.Endpoint,
		cfg.AccessKey,
		cfg.SecretKey,
		cfg.Bucket,
		cfg.UseSSL,
		cfg.PresignTTLSec,
		cfg.PresignEndpoint,
	)
}
