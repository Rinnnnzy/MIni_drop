package storage

import "io"

// Storage 是对象存储的抽象接口。
// apiserver 只依赖这个接口，不关心底层是 MinIO 还是腾讯云 COS。
// 目前只实现 MinIO，未来可以增加 COS 实现而不修改业务代码。
type Storage interface {
	// PutFile 上传本地文件到对象存储，key 是存储路径（如 /tid/perf.data）
	PutFile(key string, filePath string) error

	// PutObject 上传内存中的数据流到对象存储
	PutObject(key string, reader io.Reader, size int64, contentType string) error

	// GetObject 从对象存储下载文件，返回数据流
	GetObject(key string) (io.ReadCloser, error)

	// PreSign 生成临时访问 URL，ttlSec 是有效期（秒）
	PreSign(key string, ttlSec int) (string, error)

	// Delete 删除对象存储中的文件
	Delete(key string) error

	// IsExist 检查文件是否存在
	IsExist(key string) (bool, error)

	// ListObjects 列出指定前缀下的所有文件 key
	ListObjects(prefix string) ([]string, error)
}
