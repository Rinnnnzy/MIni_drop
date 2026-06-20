# Mini-Drop

> 一个分布式 Linux 性能采集与分析平台，支持按需对目标进程进行 CPU 热点分析、火焰图生成与智能归因建议。

---

## 一、简介

Mini-Drop 复刻自腾讯内部性能分析平台 Drop，具备完整的"采集 → 存储 → 分析 → 可视化"闭环。用户通过 Web UI 指定目标进程 PID，系统自动调度 Agent 完成 perf 采集，上传原始数据后由 Analyzer 生成交互式火焰图，并通过规则引擎和 LLM 给出热点函数的优化建议。

**核心特性：**

- 🔥 **火焰图可视化**：perf 原始数据经 FlameGraph 工具链渲染为可交互 SVG，浏览器内直接展示
- 🤖 **智能归因**：内置 8 条规则引擎（malloc 热点、mutex 竞争、memcpy 密集等）+ 可选接入 LLM，对 Top-10 热点函数给出优化建议
- 📡 **分布式架构**：Server 与 Agent 通过 gRPC 双向通信，支持多 Agent 同时在线、任务并发下发
- 🐳 **一键部署**：`docker compose up -d --build` 启动全部 8 个服务

---

## 二、演示效果

### 登录与主页

<!-- 截图：登录页面 -->

<!-- 截图：主页 Agent 列表 + 任务列表 -->

### 创建采集任务

<!-- 截图：新建任务弹窗，填写 PID / 采样时长 / 采样率 -->

### 火焰图展示

<!-- 截图：任务结果页 - 火焰图 Tab -->

### 智能归因建议

<!-- 截图：任务结果页 - 分析建议 Tab，规则建议 + AI 建议 -->

---

## 三、项目模块与技术栈

### 架构总览

```
浏览器
  │  HTTP (REST)
  ▼
web_frontend ──→ apiserver ──gRPC──→ drop_server ──gRPC──→ drop_agent
                    │                                          │
                    │                                     perf 采集
                    │                                          │
                    ▼                                          ▼
                PostgreSQL                                  MinIO
                    ▲                                          │
                    │                                    上传 perf.data
                    └──────────── analysis ◄───────────────────┘
                                 (poll_worker)
                                      │
                              生成火焰图 SVG
                              写入分析建议
```

### 各模块说明

| 模块 | 目录 | 语言 | 职责 |
|------|------|------|------|
| **Web 前端** | `web_frontend/` | React 18 + Ant Design 5 | 任务管理、Agent 列表、火焰图展示、分析建议 |
| **API Server** | `apiserver/` | Go 1.25 + Gin | HTTP REST API、任务状态机、gRPC 下发、对象存储管理 |
| **Drop Server** | `drop/` | C++ 17 + gRPC | Agent 注册中心、任务调度、采集结果中转 |
| **Drop Agent** | `drop/` | C++ 17 + gRPC | perf 采集、上传 perf.data 到 MinIO、心跳上报 |
| **Analyzer** | `analysis/` | Python 3 | perf.data 解析、FlameGraph 生成、规则匹配、LLM 建议 |
| **PostgreSQL** | — | — | 任务记录、Agent 信息、分析建议持久化 |
| **MinIO** | — | — | 对象存储，保存 perf.data 和火焰图 SVG |

### 技术栈

| 层次 | 技术选型 |
|------|----------|
| 前端框架 | React 18 / Vite / Ant Design 5 / Zustand / React Router 6 |
| API 网关 | Go / Gin / GORM / zap 结构化日志 |
| Agent 通信 | gRPC + Protobuf（双向流：HealthCheck + Hotmethod） |
| 性能采集 | Linux perf（用户态 + 内核态 CPU 采样） |
| 数据分析 | Python / stackcollapse-perf.pl / flamegraph.pl |
| LLM 接入 | OpenAI-compatible API（支持 Gemini / GPT 等） |
| 数据库 | PostgreSQL 14 |
| 对象存储 | MinIO（S3 兼容） |
| 容器化 | Docker / docker compose（8 服务一键启动） |
| 服务间通信 | gRPC（C++ ↔ Go）/ HTTP REST（Go ↔ 前端）/ psycopg2（Python ↔ PostgreSQL） |

---

## 四、部署

### 环境要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Linux（推荐 Ubuntu 22.04）|
| 内核版本 | ≥ 5.4（perf 采集宿主机进程需要） |
| **宿主机需预装** | `sudo apt install linux-tools-common linux-tools-generic`（容器不再内置 perf，运行时挂载宿主机的，详见下方说明） |
| Docker | ≥ 20.10 |
| Docker Compose | ≥ 2.0（使用 `docker compose` 而非 `docker-compose`） |
| 权限 | drop_agent / analysis 容器需要 `privileged` 权限以运行 perf |
| 内存 | ≥ 4 GB（C++ 编译阶段峰值较高） |
| 磁盘 | ≥ 10 GB（镜像 + perf 数据） |

> **为什么不在镜像里装 perf？**
> `linux-tools-generic` 装的是"构建镜像时那台机器的内核版本对应的 perf"，如果实际运行的宿主机内核版本不一致，perf 会直接报错或解析失败。本项目改为**运行时挂载宿主机的 `/usr/bin/perf` 和 `/usr/lib/linux-tools` 目录**到容器里（见 `docker-compose.yml` 的 `drop_agent` / `analysis` 服务），这样无论在哪台机器上跑，perf 版本永远和宿主机内核匹配。**唯一前提是宿主机自己要装好对应内核版本的 `linux-tools-generic`。**

> **必须在宿主机执行（容器内权限再大也绕不开这条内核开关）：**
> ```bash
> sudo sh -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid'
> ```
> 如需重启后依然生效，写入 `/etc/sysctl.conf`：`kernel.perf_event_paranoid = 1`，然后 `sudo sysctl -p`。

### 一、克隆仓库

```bash
git clone <仓库地址>
cd Mini_drop
```

### 二、配置 LLM（可选）

如需启用 AI 分析建议，在项目根目录创建 `.env` 文件：

```bash
# Mini_drop/.env
LLM_API_KEY=sk-your-api-key
LLM_BASE_URL=https://api.apifast.tech/v1
LLM_MODEL=gemini-1.5-flash
```

不配置此文件时，规则建议仍正常工作，AI 建议字段为空。

### 三、一键启动

```bash
docker compose up -d --build
```

首次构建需要编译 C++ 代码，预计耗时 **10~15 分钟**。

### 四、验证服务

```bash
# 查看所有容器状态（均应为 Up）
docker compose ps

# 查看日志
docker compose logs -f apiserver
docker compose logs -f analysis
```

### 五、访问系统

| 服务 | 地址 |
|------|------|
| Web 前端 | http://localhost |
| API Server | http://localhost:8191 |
| MinIO 控制台 | http://localhost:9001（用户名/密码：drop / dropdrop） |

### 六、使用说明

1. 打开 http://localhost，点击登录
2. 在主页确认 Agent 列表中出现在线 Agent
3. 点击「新建任务」，填写目标进程 PID（可用 `ps aux` 查找）、采样时长（建议 30s）、采样率（建议 99 Hz）
4. 等待任务状态变为「DONE」（约 30~60 秒）
5. 点击任务进入结果页，查看火焰图与分析建议

### 七、停止服务

```bash
# 停止并保留数据
docker compose down

# 停止并清空所有数据（慎用）
docker compose down -v
```

### 八、常见问题

**perf 采集失败 / 没有数据**

```bash
# 在宿主机执行，降低 perf 限制
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid
echo 0   | sudo tee /proc/sys/kernel/kptr_restrict
```

**火焰图页面空白**

MinIO 预签名 URL 默认使用 `localhost:9000`，确认宿主机 9000 端口未被占用。

**drop_agent 未出现在 Agent 列表**

Agent 首次注册需要 drop_server 和 apiserver 均就绪，等待约 30 秒后刷新页面。

---

## 许可证

本项目仅供学习与竞赛用途。
