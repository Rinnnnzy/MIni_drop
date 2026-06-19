FROM golang:1.25 AS builder

WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -o apiserver ./main.go

FROM alpine:3.19
RUN apk add --no-cache ca-certificates tzdata

WORKDIR /app
COPY --from=builder /app/apiserver .
COPY apiserver.yaml.example apiserver.yaml

EXPOSE 8191
CMD ["./apiserver", "-c", "apiserver.yaml"]
