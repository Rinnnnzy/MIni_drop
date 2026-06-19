import React from 'react'
import { Form, Input, Button, Card, Typography, message } from 'antd'
import { useNavigate } from 'react-router-dom'

const { Title, Text } = Typography

export default function Login() {
  const navigate = useNavigate()

  const onFinish = ({ name }) => {
    const trimmed = name?.trim()
    if (!trimmed) { message.error('请输入用户名'); return }

    const uid = 'user_' + Date.now()
    document.cookie = `drop_user_uid=${encodeURIComponent(uid)}; path=/`
    document.cookie = `drop_user_name=${encodeURIComponent(trimmed)}; path=/`
    message.success('登录成功')
    navigate('/index', { replace: true })
  }

  return (
    <div style={{
      display: 'flex', flexDirection: 'column',
      justifyContent: 'center', alignItems: 'center',
      height: '100vh', background: '#f0f2f5',
    }}>
      <Card style={{ width: 420, boxShadow: '0 4px 24px rgba(0,0,0,0.08)' }}>
        <div style={{ textAlign: 'center', marginBottom: 32 }}>
          <Title level={2} style={{ margin: 0 }}>🔥 Mini-Drop</Title>
          <Text type="secondary">分布式性能分析平台</Text>
        </div>
        <Form onFinish={onFinish} layout="vertical" size="large">
          <Form.Item
            label="用户名"
            name="name"
            rules={[{ required: true, message: '请输入用户名' }]}
          >
            <Input placeholder="输入你的名字" autoFocus />
          </Form.Item>
          <Form.Item style={{ marginBottom: 0 }}>
            <Button type="primary" htmlType="submit" block>进入系统</Button>
          </Form.Item>
        </Form>
      </Card>
    </div>
  )
}
