import axios from 'axios'

const request = axios.create({
  baseURL: '/',
  timeout: 30000,
  withCredentials: true,
})

function getCookie(name) {
  const match = document.cookie.match(new RegExp('(?:^|; )' + name + '=([^;]*)'))
  return match ? decodeURIComponent(match[1]) : ''
}

// 每次请求自动带上用户身份 Header
request.interceptors.request.use(cfg => {
  cfg.headers['Drop_user_uid'] = getCookie('drop_user_uid')
  cfg.headers['Drop_user_name'] = getCookie('drop_user_name')
  return cfg
})

// 统一处理 401 → 跳登录页
request.interceptors.response.use(
  res => res.data,
  err => {
    if (err.response?.status === 401) {
      window.location.href = '/login'
    }
    return Promise.reject(err)
  }
)

// ── Auth ──────────────────────────────────────────────────────────────────
export const checkAuth = () => request.get('/auth/check')

// ── User ──────────────────────────────────────────────────────────────────
export const getMe = () => request.get('/api/v1/users/me')

// ── Tasks ─────────────────────────────────────────────────────────────────
export const createTask    = (data) => request.post('/api/v1/tasks', data)
export const listTasks     = ()     => request.get('/api/v1/tasks')
export const getTask       = (tid)  => request.get(`/api/v1/tasks/${tid}`)
export const deleteTask    = (tid)  => request.delete(`/api/v1/tasks/${tid}`)
export const retryTask     = (tid)  => request.post(`/api/v1/tasks/${tid}/retry`)
export const getSuggestions = (tid) => request.get(`/api/v1/tasks/${tid}/suggestions`)
export const listCosFiles  = (tid)  => request.get('/api/v1/cosfiles', { params: { tid } })

// ── Agents ────────────────────────────────────────────────────────────────
export const listAgents = () => request.get('/api/v1/agents')
