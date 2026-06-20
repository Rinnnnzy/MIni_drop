# syntax=docker/dockerfile:1.4
FROM golang:1.25 AS builder

# 使用官方 Go 模块代理（VPN 环境下比国内镜像更稳定，避免两者路由冲突）
ENV GOPROXY=https://proxy.golang.org,direct

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

RUN apk add --no-cache ca-certificates tzdata wget

WORKDIR /app
COPY --from=builder /app/apiserver .
COPY apiserver.yaml.example apiserver.yaml

EXPOSE 8191
CMD ["./apiserver", "-c", "apiserver.yaml"]
