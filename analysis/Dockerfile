FROM ubuntu:22.04

# 换阿里云镜像源，加速 apt-get
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.aliyun.com|g; \
            s|http://security.ubuntu.com|http://mirrors.aliyun.com|g' \
    /etc/apt/sources.list

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    python3 python3-pip \
    perl \
    && rm -rf /var/lib/apt/lists/*

# 不装 linux-tools-generic：perf script 解析 perf.data 时，解析器版本要尽量匹配
# 采集时的内核版本。同 drop/Dockerfile，改为运行时挂载宿主机 /usr/bin/perf +
# /usr/lib/linux-tools（见 docker-compose.yml analysis 服务）。

WORKDIR /app
COPY requirements.txt .

# 换清华 PyPI 镜像源
RUN pip3 install --no-cache-dir -i https://mirrors.aliyun.com/pypi/simple -r requirements.txt

COPY . .
RUN chmod +x tools/flamegraph.pl tools/stackcollapse-perf.pl

CMD ["python3", "poll_worker.py"]
