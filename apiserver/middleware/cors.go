package middleware

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// CORS 处理跨域请求。
// 必须在所有路由之前注册，并且单独处理 OPTIONS 预检请求。
// withCredentials=true 时，Allow-Origin 不能为 "*"，必须回显请求的 Origin。
func CORS() gin.HandlerFunc {
	return func(c *gin.Context) {
		origin := c.Request.Header.Get("Origin")
		if origin != "" {
			c.Header("Access-Control-Allow-Origin", origin)
			c.Header("Access-Control-Allow-Credentials", "true")
			c.Header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
			c.Header("Access-Control-Allow-Headers", "Content-Type, Drop_user_uid, Drop_user_name")
			c.Header("Access-Control-Max-Age", "86400")
		}

		if c.Request.Method == http.MethodOptions {
			c.AbortWithStatus(http.StatusNoContent)
			return
		}

		c.Next()
	}
}
