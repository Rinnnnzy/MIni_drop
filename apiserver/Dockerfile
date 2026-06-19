FROM golang:1.25 AS builder

# 换国内 Go 代理，加速 go mod download
ENV GOPROXY=https://goproxy.cn,direct

WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -o apiserver ./main.go

FROM alpine:3.19

# 换阿里云 Alpine 镜像源
RUN sed -i 's|dl-cdn.alpinelinux.org|mirrors.aliyun.com|g' /etc/apk/repositories

RUN apk add --no-cache ca-certificates tzdata wget

WORKDIR /app
COPY --from=builder /app/apiserver .
COPY apiserver.yaml.example apiserver.yaml

EXPOSE 8191
CMD ["./apiserver", "-c", "apiserver.yaml"]
