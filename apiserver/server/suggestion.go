package server

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"

	"minidrop/apiserver/model"
	"minidrop/apiserver/util"
)

// GetSuggestions GET /api/v1/tasks/:tid/suggestions
// 返回该任务的所有热点函数分析建议（规则建议 + AI 建议）。
func (s *APIServer) GetSuggestions(c *gin.Context) {
	tid := c.Param("tid")

	var list []model.AnalysisSuggestion
	if err := s.DB.Where("tid = ?", tid).Find(&list).Error; err != nil {
		util.Logger.Error("GetSuggestions: db error", zap.String("tid", tid), zap.Error(err))
		fail(c, http.StatusInternalServerError, CodeServerError, "db error")
		return
	}

	ok(c, gin.H{"suggestions": list})
}
