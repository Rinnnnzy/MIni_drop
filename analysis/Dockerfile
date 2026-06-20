# syntax=docker/dockerfile:1.4
FROM ubuntu:22.04

# --mount=type=cache：apt 包缓存持久化，避免每次重建都重新下载
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    python3 python3-pip \
    perl \
    libelf1 libdw1 libunwind8 libslang2 libnuma1 \
    libtraceevent1 zlib1g liblzma5 libbz2-1.0 libexpat1

# 不装 linux-tools-generic：同 drop/Dockerfile，改为运行时挂载宿主机真实 perf 二进制
# （HOST_PERF_PATH，见 docker-compose.yml）。上面这些 lib 是 perf 依赖的用户态共享库，
# 跟内核版本无关，apt 装一次永久有效，不存在"换机器又失效"的问题。

WORKDIR /app
COPY requirements.txt .

# pip 缓存同样持久化，避免每次重新下载 wheel 包
RUN --mount=type=cache,target=/root/.cache/pip \
    pip3 install -r requirements.txt

COPY . .
RUN chmod +x tools/flamegraph.pl tools/stackcollapse-perf.pl

CMD ["python3", "poll_worker.py"]
