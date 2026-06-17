#pragma once
#include <string>
#include <vector>

namespace dropd {

// COSClient 封装通过 mc（MinIO Client）CLI 把本地文件上传到 MinIO 的操作。
//
// Day 4 选用 mc cp 而非 C++ S3 SDK 的原因：
//   aws-sdk-cpp / minio-cpp 需要额外编译，加重 Day 4 环境配置负担；
//   mc 是官方工具，行为与文档一致，调试方便，代价只是 Dockerfile 多装 mc。
//
// 使用流程：
//   COSClient cos(endpoint, ak, sk, bucket, false);
//   std::string err;
//   if (!cos.Init(&err)) { ... }
//   if (!cos.PutFile("/tmp/perf.data", "tid/perf.data", &err)) { ... }
class COSClient {
public:
    // endpoint: MinIO 地址，例如 "minio:9000"（不含协议头，由 use_ssl 决定 http/https）
    COSClient(const std::string& endpoint,
              const std::string& access_key,
              const std::string& secret_key,
              const std::string& bucket,
              bool use_ssl);

    // Init 执行 "mc alias set ..." 向 mc 注册连接别名。幂等，多次调用安全。
    bool Init(std::string* err_msg);

    // PutFile 把 local_path 指向的本地文件上传到 MinIO 桶内的 cos_key 路径。
    // cos_key 不含桶名前缀，例如 "tid/perf.data"。
    bool PutFile(const std::string& local_path,
                 const std::string& cos_key,
                 std::string* err_msg);

private:
    std::string endpoint_;
    std::string access_key_;
    std::string secret_key_;
    std::string bucket_;
    bool        use_ssl_;

    // mc alias 名称，硬编码避免与用户已有 alias 冲突
    static const char kAlias[];

    // RunCommand fork+exec 执行命令并等待完成，返回退出码（-1 表示 fork/exec 本身失败）
    static int RunCommand(const std::vector<std::string>& args);
};

} // namespace dropd
