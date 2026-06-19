import React, { useEffect, useState } from 'react'
import {
  Layout, Descriptions, Tag, Tabs, Table,
  Typography, Spin, Alert, Button, Space,
} from 'antd'
import { useSearchParams, useNavigate } from 'react-router-dom'
import AppHeader from '../../components/header'
import Flamegraph from '../../components/flamegraph'
import { getTask, listCosFiles, getSuggestions } from '../../api'

const { Content } = Layout

const STATUS_MAP = {
  0: { text: '待处理', color: 'default' },
  1: { text: '采集中', color: 'processing' },
  2: { text: '完成',   color: 'success' },
  3: { text: '失败',   color: 'error' },
}

const ANALYSIS_MAP = {
  0: { text: '待分析', color: 'default' },
  1: { text: '分析中', color: 'processing' },
  2: { text: '已完成', color: 'success' },
  3: { text: '失败',   color: 'error' },
}

export default function TaskResult() {
  const [params]  = useSearchParams()
  const tid       = params.get('tid')
  const navigate  = useNavigate()

  const [task, setTask]               = useState(null)
  const [flamegraphUrl, setFlamegraph] = useState('')
  const [suggestions, setSuggestions] = useState([])

  const fetchTask = () =>
    getTask(tid).then(res => setTask(res.data)).catch(() => {})

  const fetchSuggestions = () =>
    getSuggestions(tid).then(res => setSuggestions(res.data?.suggestions || [])).catch(() => {})

  const fetchFlamegraph = () =>
    listCosFiles(tid).then(res => {
      const svg = (res.data?.files || []).find(f => f.filename.endsWith('flamegraph.svg'))
      if (svg) setFlamegraph(svg.url)
    }).catch(() => {})

  // 首次加载
  useEffect(() => {
    if (!tid) return
    fetchTask()
    fetchSuggestions()
  }, [tid])

  // 采集完成后拉火焰图
  useEffect(() => {
    if (task?.status === 2) fetchFlamegraph()
  }, [task?.status])

  // 轮询：任务未完成时每 3 秒刷新
  useEffect(() => {
    if (!task || task.status >= 2) return
    const id = setInterval(() => {
      fetchTask()
      if (task.analysis_status < 2) fetchSuggestions()
    }, 3000)
    return () => clearInterval(id)   // ← 切页面时必须清理！
  }, [task?.status, task?.analysis_status])

  if (!task) {
    return (
      <Layout style={{ minHeight: '100vh' }}>
        <AppHeader />
        <Content style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: 400 }}>
          <Spin size="large" tip="加载中…" />
        </Content>
      </Layout>
    )
  }

  const suggColumns = [
    { title: '热点函数',  dataIndex: 'func',          key: 'func',          width: 220, ellipsis: true },
    { title: '规则建议',  dataIndex: 'suggestion',    key: 'suggestion',    ellipsis: true },
    { title: 'AI 建议',   dataIndex: 'ai_suggestion', key: 'ai_suggestion', ellipsis: true },
  ]

  const tabItems = [
    {
      key: 'info',
      label: '基本信息',
      children: (
        <Descriptions bordered column={2} size="small">
          <Descriptions.Item label="任务 ID">{task.tid}</Descriptions.Item>
          <Descriptions.Item label="名称">{task.name || '—'}</Descriptions.Item>
          <Descriptions.Item label="目标 IP">{task.target_ip}</Descriptions.Item>
          <Descriptions.Item label="采集状态">
            <Tag color={STATUS_MAP[task.status]?.color}>{STATUS_MAP[task.status]?.text}</Tag>
          </Descriptions.Item>
          <Descriptions.Item label="分析状态">
            <Tag color={ANALYSIS_MAP[task.analysis_status]?.color}>{ANALYSIS_MAP[task.analysis_status]?.text}</Tag>
          </Descriptions.Item>
          <Descriptions.Item label="状态详情" span={2}>{task.status_info || '—'}</Descriptions.Item>
          <Descriptions.Item label="创建时间">
            {task.create_time ? new Date(task.create_time).toLocaleString('zh-CN') : '—'}
          </Descriptions.Item>
          <Descriptions.Item label="结束时间">
            {task.end_time ? new Date(task.end_time).toLocaleString('zh-CN') : '—'}
          </Descriptions.Item>
          <Descriptions.Item label="采集文件路径" span={2}>{task.cos_key || '—'}</Descriptions.Item>
        </Descriptions>
      ),
    },
    {
      key: 'flamegraph',
      label: '火焰图',
      children: task.status < 2
        ? <Alert message={`任务${STATUS_MAP[task.status]?.text}，火焰图生成后自动显示`} type="info" showIcon style={{ margin: '24px 0' }} />
        : <Flamegraph url={flamegraphUrl} />,
    },
    {
      key: 'suggestions',
      label: `AI 建议${suggestions.length ? ` (${suggestions.length})` : ''}`,
      children: suggestions.length === 0
        ? <Alert message="暂无分析建议，分析完成后自动更新" type="info" showIcon style={{ margin: '24px 0' }} />
        : (
          <Table
            dataSource={suggestions}
            columns={suggColumns}
            rowKey="id"
            size="small"
            pagination={false}
            expandable={{
              expandedRowRender: row => (
                <Space direction="vertical" style={{ padding: '8px 0', width: '100%' }}>
                  {row.suggestion    && <><strong>规则建议：</strong>{row.suggestion}</>}
                  {row.ai_suggestion && <><strong>AI 建议：</strong>{row.ai_suggestion}</>}
                </Space>
              ),
              rowExpandable: row => !!(row.suggestion || row.ai_suggestion),
            }}
          />
        ),
    },
  ]

  return (
    <Layout style={{ minHeight: '100vh', background: '#f5f5f5' }}>
      <AppHeader />
      <Content style={{ padding: 24, maxWidth: 1200, margin: '0 auto', width: '100%' }}>
        <Space style={{ marginBottom: 16 }}>
          <Button onClick={() => navigate(-1)}>← 返回</Button>
          <Typography.Title level={4} style={{ margin: 0 }}>
            任务详情 — <Typography.Text code>{tid}</Typography.Text>
          </Typography.Title>
        </Space>

        {task.status < 2 && (
          <Alert
            message="任务进行中，页面每 3 秒自动刷新"
            type="info" showIcon closable
            style={{ marginBottom: 16 }}
          />
        )}

        <div style={{ background: '#fff', padding: 24, borderRadius: 8 }}>
          <Tabs items={tabItems} defaultActiveKey="info" />
        </div>
      </Content>
    </Layout>
  )
}
