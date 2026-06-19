import React, { useEffect, useState } from 'react'
import { useNavigate, useLocation } from 'react-router-dom'
import { Spin } from 'antd'
import Router from './router'
import { checkAuth } from './api'
import useStore from './store'

export default function App() {
  const [loading, setLoading] = useState(true)
  const setUser = useStore(s => s.setUser)
  const navigate = useNavigate()
  const location = useLocation()

  useEffect(() => {
    // 登录页不需要 auth check
    if (location.pathname === '/login') {
      setLoading(false)
      return
    }
    checkAuth()
      .then(res => {
        setUser(res.data)
        setLoading(false)
      })
      .catch(() => {
        navigate('/login', { replace: true })
        setLoading(false)
      })
  }, [])

  if (loading) {
    return (
      <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '100vh' }}>
        <Spin size="large" />
      </div>
    )
  }

  return <Router />
}
