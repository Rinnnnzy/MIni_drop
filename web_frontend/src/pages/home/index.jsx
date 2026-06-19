import React, { useEffect, useState } from 'react'
import { Layout, Table, Tag, Button, Space, Typography, message } from 'antd'
import { useNavigate } from 'react-router-dom'
import AppHeader from '../../components/header'
import CreateTaskModal from '../../components/createTaskModal'
import { listAgents, listTasks } from '../../api'

const { Content } = Layout

const STATUS_MAP = {
  0: { text: '待处理', color: 'default' },
  1: { text: '采集中', color: 'processing' },
  2: { text: '完成',   color: 'success' },
  3: { text: '失败',   color: 'error' },
}

export default function Home() {
  const [agents, setAgents]     = useState([])
  const [tasks, setTasks]       = useState([])
  const [modalOpen, setModalOpen] = useState(false)
  const navigate = useNavigate()

  const loadData = () => {
    listAgents().then(res => setAgents(res.data?.agents || [])).catch(() => {})
    listTasks().then(res  => setTasks(res.data?.tasks   || [])).catch(() => {})
  }

  useEffect(() => { loadData() }, [])

  const agentColumns = [
    { title: 'IP 地址',  dataIndex: 'ip_addr',   key: 'ip_addr' },
    { title: '主机名',   dataIndex: 'hostname',   key: 'hostname' },
    { title: '版本',     dataIndex: 'version',    key: 'version' },
    {
      title: '状态', dataIndex: 'online', key: 'online',
      render: v => <Tag color={v ? 'success' : 'default'}>{v ? '在线' : '离线'}</Tag>,
    },
    {
      title: '最后心跳', dataIndex: 'last_seen', key: 'last_seen',
      render: v => v ? new Date(v).toLocaleString('zh-CN') : '—',
    },
  ]

  const taskColumns = [
    { title: '任务 ID',  dataIndex: 'tid',       key: 'tid',       width: 100 },
    { title: '名称',     dataIndex: 'name',      key: 'name' },
    { title: '目标 IP',  dataIndex: 'target_ip', key: 'target_ip' },
    {
      title: '状态', dataIndex: 'status', key: 'status',
      render: v => <Tag color={STATUS_MAP[v]?.color}>{STATUS_MAP[v]?.text}</Tag>,
    },
    {
      title: '创建时间', dataIndex: 'create_time', key: 'create_time',
      render: v => v ? new Date(v).toLocaleString('zh-CN') : '—',
    },
    {
      title: '操作', key: 'action',
      render: (_, row) => (
        <Button type="link" size="small" onClick={() => navigate(`/task/result?tid=${row.tid}`)}>
          查看详情
        </Button>
      ),
    },
  ]

  return (
    <Layout style={{ minHeight: '100vh', background: '#f5f5f5' }}>
      <AppHeader />
      <Content style={{ padding: 24, maxWidth: 1200, margin: '0 auto', width: '100%' }}>
        <Space direction="vertical" style={{ width: '100%' }} size={24}>

          {/* Agent 列表 */}
          <div style={{ background: '#fff', padding: 24, borderRadius: 8 }}>
            <Typography.Title level={5} style={{ marginTop: 0 }}>Agent 列表</Typography.Title>
            <Table
              dataSource={agents}
              columns={agentColumns}
              rowKey="id"
              size="small"
              pagination={false}
              locale={{ emptyText: '暂无 Agent，请先启动 drop_agent' }}
            />
          </div>

          {/* 我的任务 */}
          <div style={{ background: '#fff', padding: 24, borderRadius: 8 }}>
            <Space style={{ marginBottom: 16, justifyContent: 'space-between', width: '100%' }}>
              <Typography.Title level={5} style={{ margin: 0 }}>我的任务</Typography.Title>
              <Button type="primary" onClick={() => setModalOpen(true)}>+ 新建采样</Button>
            </Space>
            <Table
              dataSource={tasks}
              columns={taskColumns}
              rowKey="tid"
              size="small"
              pagination={{ pageSize: 10, showSizeChanger: false }}
              locale={{ emptyText: '暂无任务，点击"新建采样"开始' }}
            />
          </div>

        </Space>
      </Content>

      <CreateTaskModal
        open={modalOpen}
        onClose={() => setModalOpen(false)}
        onCreated={() => { setModalOpen(false); loadData() }}
      />
    </Layout>
  )
}
