import React from 'react'
import { Layout, Space, Typography, Button } from 'antd'
import { useNavigate, useLocation } from 'react-router-dom'
import useStore from '../../store'

const { Header } = Layout

const NAV_ITEMS = [
  { path: '/index', label: '主页' },
  { path: '/tasks', label: '全部任务' },
]

export default function AppHeader() {
  const user = useStore(s => s.user)
  const navigate = useNavigate()
  const location = useLocation()

  const logout = () => {
    document.cookie = 'drop_user_uid=; path=/; max-age=0'
    document.cookie = 'drop_user_name=; path=/; max-age=0'
    navigate('/login', { replace: true })
  }

  return (
    <Header style={{
      display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      padding: '0 24px', background: '#001529', position: 'sticky', top: 0, zIndex: 100,
    }}>
      <Space size={32}>
        <Typography.Text
          style={{ color: '#fff', fontSize: 18, fontWeight: 700, cursor: 'pointer', whiteSpace: 'nowrap' }}
          onClick={() => navigate('/index')}
        >
          🔥 Mini-Drop
        </Typography.Text>
        <Space>
          {NAV_ITEMS.map(item => (
            <Button
              key={item.path}
              type="link"
              style={{ color: location.pathname === item.path ? '#1677ff' : '#ccc', padding: '0 4px' }}
              onClick={() => navigate(item.path)}
            >
              {item.label}
            </Button>
          ))}
        </Space>
      </Space>
      <Space>
        <Typography.Text style={{ color: '#aaa' }}>{user?.user_name}</Typography.Text>
        <Button size="small" onClick={logout}>退出</Button>
      </Space>
    </Header>
  )
}
