package middleware

import (
	"time"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"

	"minidrop/apiserver/util"
)

// AccessLog 是 Gin 的访问日志中间件。
// 每个 HTTP 请求完成后打印一行 JSON 日志，包含方法、路径、状态码、耗时、客户端 IP。
func AccessLog() gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()
		path := c.Request.URL.Path

		c.Next() // 执行后续 handler

		util.Logger.Info("access",
			zap.String("method", c.Request.Method),
			zap.String("path", path),
			zap.Int("status", c.Writer.Status()),
			zap.Duration("latency", time.Since(start)),
			zap.String("client_ip", c.ClientIP()),
			zap.String("error", c.Errors.ByType(gin.ErrorTypePrivate).String()),
		)
	}
}
