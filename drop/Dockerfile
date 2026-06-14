FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    linux-perf perl libgrpc++1 libprotobuf23 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/build/drop_server /usr/local/bin/drop_server
COPY --from=builder /build/build/drop_agent  /usr/local/bin/drop_agent
COPY etc/config.json.example /etc/drop/config.json
