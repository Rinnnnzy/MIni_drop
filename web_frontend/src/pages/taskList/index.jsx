import React, { useEffect, useState } from 'react'
import { Layout, Table, Tag, Button, Space, Input, Popconfirm, Typography, message } from 'antd'
import { useNavigate } from 'react-router-dom'
import AppHeader from '../../components/header'
import { listTasks, deleteTask } from '../../api'

const { Content } = Layout
const { Search } = Input

const STATUS_MAP = {
  0: { text: '待处理', color: 'default' },
  1: { text: '采集中', color: 'processing' },
  2: { text: '上传中', color: 'processing' },
  3: { text: '完成',   color: 'success' },
  4: { text: '失败',   color: 'error' },
}

const ANALYSIS_MAP = {
  0: { text: '待分析', color: 'default' },
  1: { text: '分析中', color: 'processing' },
  2: { text: '已完成', color: 'success' },
  3: { text: '失败',   color: 'error' },
}

export default function TaskList() {
  const [all, setAll]           = useState([])
  const [filtered, setFiltered] = useState([])
  const navigate = useNavigate()

  const load = () => {
    listTasks()
      .then(res => { const d = res.data?.tasks || []; setAll(d); setFiltered(d) })
      .catch(() => {})
  }

  useEffect(() => { load() }, [])

  const onSearch = (val) => {
    if (!val) { setFiltered(all); return }
    const kw = val.toLowerCase()
    setFiltered(all.filter(t =>
      t.tid.includes(kw) ||
      t.target_ip.includes(kw) ||
      (t.name || '').toLowerCase().includes(kw)
    ))
  }

  const onDelete = async (tid) => {
    try {
      await deleteTask(tid)
      message.success('已删除')
      load()
    } catch {
      message.error('删除失败')
    }
  }

  const columns = [
    { title: '任务 ID',  dataIndex: 'tid',       key: 'tid',       width: 100 },
    { title: '名称',     dataIndex: 'name',      key: 'name',      ellipsis: true },
    { title: '目标 IP',  dataIndex: 'target_ip', key: 'target_ip', width: 140 },
    {
      title: '采集状态', dataIndex: 'status', key: 'status', width: 100,
      render: v => <Tag color={STATUS_MAP[v]?.color}>{STATUS_MAP[v]?.text}</Tag>,
    },
    {
      title: '分析状态', dataIndex: 'analysis_status', key: 'analysis_status', width: 100,
      render: v => <Tag color={ANALYSIS_MAP[v]?.color}>{ANALYSIS_MAP[v]?.text}</Tag>,
    },
    {
      title: '创建时间', dataIndex: 'create_time', key: 'create_time', width: 180,
      render: v => v ? new Date(v).toLocaleString('zh-CN') : '—',
    },
    {
      title: '操作', key: 'action', width: 120,
      render: (_, row) => (
        <Space size={0}>
          <Button
            type="link" size="small"
            onClick={() => navigate(`/task/result?tid=${row.tid}`)}
          >
            详情
          </Button>
          <Popconfirm
            title="确认删除该任务？"
            okText="删除" cancelText="取消"
            okButtonProps={{ danger: true }}
            onConfirm={() => onDelete(row.tid)}
          >
            <Button type="link" size="small" danger>删除</Button>
          </Popconfirm>
        </Space>
      ),
    },
  ]

  return (
    <Layout style={{ minHeight: '100vh', background: '#f5f5f5' }}>
      <AppHeader />
      <Content style={{ padding: 24, maxWidth: 1200, margin: '0 auto', width: '100%' }}>
        <div style={{ background: '#fff', padding: 24, borderRadius: 8 }}>
          <Space style={{ marginBottom: 16, justifyContent: 'space-between', width: '100%' }}>
            <Typography.Title level={4} style={{ margin: 0 }}>全部任务</Typography.Title>
            <Search
              placeholder="搜索 ID / IP / 名称"
              onSearch={onSearch}
              onChange={e => !e.target.value && setFiltered(all)}
              allowClear
              style={{ width: 280 }}
            />
          </Space>
          <Table
            dataSource={filtered}
            columns={columns}
            rowKey="tid"
            pagination={{ pageSize: 20, showTotal: t => `共 ${t} 条` }}
          />
        </div>
      </Content>
    </Layout>
  )
}
