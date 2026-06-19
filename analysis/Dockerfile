FROM ubuntu:22.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    python3 python3-pip \
    linux-tools-generic linux-tools-common \
    perl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements.txt .
RUN pip3 install --no-cache-dir -r requirements.txt

COPY . .
RUN chmod +x tools/flamegraph.pl tools/stackcollapse-perf.pl

# poll_worker.py polls PostgreSQL every 5s and triggers hotmethod_analyzer.py.
# Override with: docker run analysis python3 hotmethod_analyzer.py --task-id X
CMD ["python3", "poll_worker.py"]
