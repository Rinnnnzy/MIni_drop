package server

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// CheckAuth GET /auth/check
// 前端启动时调用，检查 Cookie 中是否有用户身份。
// 此接口不挂 CheckLogin 中间件，由自己处理鉴权逻辑。
func (s *APIServer) CheckAuth(c *gin.Context) {
	uid := cookieOrHeader(c, "drop_user_uid", "Drop_user_uid")
	if uid == "" {
		c.JSON(http.StatusUnauthorized, gin.H{
			"code": CodeUnauth,
			"msg":  "not logged in",
			"data": gin.H{"location": "/login"},
		})
		return
	}
	userName := cookieOrHeader(c, "drop_user_name", "Drop_user_name")
	ok(c, gin.H{"uid": uid, "user_name": userName})
}

// GetMe GET /api/v1/users/me
// 返回当前登录用户的基本信息（由 CheckLogin 中间件注入到 context）。
func (s *APIServer) GetMe(c *gin.Context) {
	ok(c, gin.H{
		"uid":       c.GetString("uid"),
		"user_name": c.GetString("user_name"),
	})
}

// cookieOrHeader 先读 Cookie，读不到再读 Header。
func cookieOrHeader(c *gin.Context, cookieKey, headerKey string) string {
	if v, err := c.Cookie(cookieKey); err == nil && v != "" {
		return v
	}
	return c.GetHeader(headerKey)
}
