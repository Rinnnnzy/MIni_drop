# ── Stage 1: Build ────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libgrpc++1 libprotobuf23 \
    linux-tools-generic linux-tools-common \
    perl curl iproute2 \
    && rm -rf /var/lib/apt/lists/*

# MinIO client (mc) — needed by drop_agent to upload perf.data
RUN curl -sSL https://dl.min.io/client/mc/release/linux-amd64/mc \
    -o /usr/local/bin/mc && chmod +x /usr/local/bin/mc

COPY --from=builder /build/build/drop_server /usr/local/bin/drop_server
COPY --from=builder /build/build/drop_agent  /usr/local/bin/drop_agent

# Agent config — endpoint values are overridden via env vars at runtime
COPY etc/config.json.example /etc/drop/config.json

# Default: start the gRPC server.
# drop_agent uses: command: ["/usr/local/bin/drop_agent"] in docker-compose.
CMD ["/usr/local/bin/drop_server"]
