package util

import (
	"net/http"
	"time"
)

// NewHTTPClient 返回一个设置了合理超时的 HTTP 客户端。
// apiserver 调用外部 HTTP 服务（如触发 Analyzer）时使用。
func NewHTTPClient(timeoutSec int) *http.Client {
	if timeoutSec <= 0 {
		timeoutSec = 30
	}
	return &http.Client{
		Timeout: time.Duration(timeoutSec) * time.Second,
	}
}
