package server

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// CheckLogin 是鉴权中间件，挂在所有需要登录的路由上。
// Day 2 的简化实现：从请求 Header 读取 Drop_user_uid 和 Drop_user_name。
// 字段缺失则返回 401，让前端跳转登录页。
// 后续可替换为 Cookie / JWT 验证。
func CheckLogin() gin.HandlerFunc {
	return func(c *gin.Context) {
		uid := c.GetHeader("Drop_user_uid")
		if uid == "" {
			// 也尝试从 Cookie 读取（兼容浏览器请求）
			if cookie, err := c.Cookie("drop_user_uid"); err == nil {
				uid = cookie
			}
		}

		if uid == "" {
			c.JSON(http.StatusUnauthorized, gin.H{
				"code": CodeUnauth,
				"msg":  "not logged in",
				"data": gin.H{"location": "/login"},
			})
			c.Abort() // 终止后续 handler 执行
			return
		}

		userName := c.GetHeader("Drop_user_name")
		if userName == "" {
			if cookie, err := c.Cookie("drop_user_name"); err == nil {
				userName = cookie
			}
		}

		// 把用户身份写入 context，后续 handler 通过 c.GetString("uid") 读取
		c.Set("uid", uid)
		c.Set("user_name", userName)
		c.Next()
	}
}
