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
| 操作系统 | Linux（推荐 Ubuntu 22.04），内核 ≥ 5.4 |
| 宿主机需预装 | `sudo apt install linux-tools-common linux-tools-generic`（容器不内置 perf，运行时挂载宿主机的，见下方注意事项） |
| Docker / Compose | Docker ≥ 20.10，Compose ≥ 2.0（用 `docker compose` 而非 `docker-compose`） |
| 内存 / 磁盘 | ≥ 4 GB 内存（C++ 编译阶段峰值高）、≥ 10 GB 磁盘 |
| 构建期网络 | 需能访问 `dl.min.io`（下载 MinIO client）。国内直连超时的话，开代理重跑 `docker compose build drop_server` 即可，其余镜像走国内源不受影响 |

### 部署步骤

```bash
git clone <仓库地址> && cd Mini_drop

# 1. 解析本机 perf 真实路径（必须，每台机器执行一次，写入 .env 后 compose 自动读取）
echo "HOST_PERF_PATH=$(readlink -f /usr/lib/linux-tools/$(uname -r)/perf)" > .env

# 2.（可选）配置 LLM，不配置则规则建议正常工作，AI 建议字段为空
cat >> .env << 'EOF'
LLM_API_KEY=你的api key
LLM_BASE_URL=你的url
LLM_MODEL=gemini-2.5-flash
EOF

# 3. 必须在宿主机放开 perf 权限（容器权限再大也绕不开这个内核开关）
sudo sh -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid'

# 4. 一键启动（首次构建耗时 10~15 分钟）
docker compose up -d --build
```

### 三个关键注意事项

1. **perf 路径不能用 `which perf` 解析**——`/usr/bin/perf` 在 Ubuntu 上是检测脚本不是软链接，必须从 `/usr/lib/linux-tools/$(uname -r)/perf` 开始 `readlink -f`，否则容器里报内核版本不匹配或缺共享库。
2. **浏览器和 Docker 不在同一台机器时**（比如本项目从 Windows 浏览器访问 Linux 虚拟机），额外在 `.env` 加一行 `MINIO_PRESIGN_HOST=<Docker所在机器的真实IP>:9000`，否则火焰图报 `SignatureDoesNotMatch`。同机场景不用配，默认 `localhost:9000` 就对。
3. **用 `export` 设置环境变量时**注意 shell 里残留的旧值优先级高于 `.env` 文件，旧值不清会导致 `.env` 改了也不生效——所以统一用 `.env` 文件，不用 `export`。

### 验证服务

```bash
# 查看所有容器状态（均应为 Up）
docker compose ps

# 查看日志
docker compose logs -f apiserver
docker compose logs -f analysis
```

### 访问系统

| 服务 | 地址 |
|------|------|
| Web 前端 | http://localhost |
| API Server | http://localhost:8191 |
| MinIO 控制台 | http://localhost:9001（用户名/密码：drop / dropdrop） |

> **在虚拟机里跑 Docker、用 Windows/Mac 主机浏览器访问的场景，请注意：**
> - **在虚拟机内部**（虚拟机自己的终端/浏览器）打开：直接用 `http://localhost` 即可。
> - **在 Windows/Mac 主机**（跑 Docker 的虚拟机之外）打开：`localhost` 对主机浏览器来说指的是主机自己，不是虚拟机，必须改用**虚拟机的真实 IP**，例如 `http://192.168.x.x`。在虚拟机里执行 `hostname -I` 可以查到这个 IP。
> - 这种"浏览器和 Docker 不在同一台机器"的场景下，还需要在 `.env` 里设置 `MINIO_PRESIGN_HOST=<虚拟机IP>:9000`，否则火焰图无法加载（详见上方"环境要求"和下方"常见问题"）。

### 使用说明

1. 打开 Web 前端地址（虚拟机内部用 `http://localhost`，虚拟机外部主机用虚拟机 IP，见上方说明）；本项目未实现账号密码登录，登录页只需输入一个用户名（用于标识身份，写入 cookie），无需注册、无需密码
2. 在主页确认 Agent 列表中出现在线 Agent
3. 点击「新建任务」，填写目标进程 PID（可用 `ps aux` 查找）、采样时长（建议 30s）、采样率（建议 99 Hz）
4. 等待任务状态变为「DONE」（约 30~60 秒）
5. 点击任务进入结果页，查看火焰图与分析建议

### 停止服务

```bash
# 停止并保留数据
docker compose down

# 停止并清空所有数据（慎用）
docker compose down -v
```

### 常见问题

**perf 采集失败 / 没有数据**

```bash
# 在宿主机执行，降低 perf 限制
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid
echo 0   | sudo tee /proc/sys/kernel/kptr_restrict
```

**火焰图页面空白 / 提示 `SignatureDoesNotMatch`**

预签名 URL 的签名包含 host，必须和浏览器实际访问 MinIO 时用的地址完全一致。同机场景确认默认值
`localhost:9000` 没被改过、9000 端口没被占用；分离场景（浏览器和 Docker 不在同一台机器）必须按上方
"环境要求"里的说明在 `.env` 配置 `MINIO_PRESIGN_HOST`，否则签名用的 host 和浏览器访问的 host 不一致会报错。

**drop_agent 未出现在 Agent 列表**

Agent 首次注册需要 drop_server 和 apiserver 均就绪，等待约 30 秒后刷新页面。

---

## 许可证

本项目仅供学习与竞赛用途。
