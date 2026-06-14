FROM ubuntu:22.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y python3 python3-pip linux-perf perl
WORKDIR /app
COPY requirements.txt .
RUN pip3 install --no-cache-dir -r requirements.txt
COPY . .
RUN chmod +x tools/flamegraph.pl tools/stackcollapse-perf.pl
ENTRYPOINT ["python3", "hotmethod_analyzer.py"]
