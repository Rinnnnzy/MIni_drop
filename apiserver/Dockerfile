# syntax=docker/dockerfile:1.4
FROM golang:1.25 AS builder

# 换国内 Go 代理，加速 go mod download
ENV GOPROXY=https://goproxy.cn,direct

WORKDIR /app
COPY go.mod go.sum ./
# --mount=type=cache：把 Go 模块缓存挂载到 BuildKit 的持久卷里，
# 不管这一层是否命中 Docker 层缓存，依赖都不会重新从网络下载。
# 这是"每次部署都要重新下载"的根本修复——之前没有任何东西跨 build 持久化。
RUN --mount=type=cache,target=/go/pkg/mod \
    go mod download

COPY . .
RUN --mount=type=cache,target=/go/pkg/mod \
    --mount=type=cache,target=/root/.cache/go-build \
    CGO_ENABLED=0 GOOS=linux go build -o apiserver ./main.go

FROM alpine:3.19

# 换阿里云 Alpine 镜像源
RUN sed -i 's|dl-cdn.alpinelinux.org|mirrors.aliyun.com|g' /etc/apk/repositories

RUN apk add --no-cache ca-certificates tzdata wget

WORKDIR /app
COPY --from=builder /app/apiserver .
COPY apiserver.yaml.example apiserver.yaml

EXPOSE 8191
CMD ["./apiserver", "-c", "apiserver.yaml"]
