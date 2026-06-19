import React from 'react'
import { Routes, Route, Navigate } from 'react-router-dom'
import Login from './pages/login'
import Home from './pages/home'
import TaskList from './pages/taskList'
import TaskResult from './pages/taskResult'

export default function Router() {
  return (
    <Routes>
      <Route path="/login"        element={<Login />} />
      <Route path="/index"        element={<Home />} />
      <Route path="/tasks"        element={<TaskList />} />
      <Route path="/task/result"  element={<TaskResult />} />
      <Route path="/"             element={<Navigate to="/index" replace />} />
    </Routes>
  )
}
