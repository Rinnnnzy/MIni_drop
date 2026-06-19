FROM ubuntu:22.04

# 换阿里云镜像源，加速 apt-get
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.aliyun.com|g; \
            s|http://security.ubuntu.com|http://mirrors.aliyun.com|g' \
    /etc/apt/sources.list

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    python3 python3-pip \
    linux-tools-generic linux-tools-common \
    perl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements.txt .

# 换清华 PyPI 镜像源
RUN pip3 install --no-cache-dir -i https://pypi.tuna.tsinghua.edu.cn/simple \
    -r requirements.txt

COPY . .
RUN chmod +x tools/flamegraph.pl tools/stackcollapse-perf.pl

CMD ["python3", "poll_worker.py"]
