# ── Stage 1: Build ────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

# 换阿里云镜像源，加速 apt-get
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.aliyun.com|g; \
            s|http://security.ubuntu.com|http://mirrors.aliyun.com|g' \
    /etc/apt/sources.list

RUN apt-get update && apt-get install -y \ 
    build-essential cmake git pkg-config \
    libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04

RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.aliyun.com|g; \
            s|http://security.ubuntu.com|http://mirrors.aliyun.com|g' \
    /etc/apt/sources.list

RUN apt-get update && apt-get install -y \
    libgrpc++1 libprotobuf23 \
    perl curl iproute2 \
    && rm -rf /var/lib/apt/lists/*

# 不在镜像里装 linux-tools-generic：它会装"当前构建环境内核版本"的 perf，
# 跟实际宿主机内核大概率不匹配，导致 perf 解析失败。
# 改为运行时从宿主机挂载 /usr/bin/perf + /usr/lib/linux-tools（见 docker-compose.yml），
# 这样不管在哪台机器上跑，perf 版本永远和宿主机内核一致。

# MinIO client (mc) — needed by drop_agent to upload perf.data
RUN curl -sSL https://dl.min.io/client/mc/release/linux-amd64/mc \
    -o /usr/local/bin/mc && chmod +x /usr/local/bin/mc

COPY --from=builder /build/build/drop_server /usr/local/bin/drop_server
COPY --from=builder /build/build/drop_agent  /usr/local/bin/drop_agent

COPY etc/config.json.example /etc/drop/config.json

CMD ["/usr/local/bin/drop_server"]
