package minio

import (
	"context"
	"io"
	"net/url"
	"time"

	"github.com/minio/minio-go/v7"
	"github.com/minio/minio-go/v7/pkg/credentials"
)

// MinIOStorage 实现 storage.Storage 接口，底层使用 MinIO。
type MinIOStorage struct {
	client        *minio.Client // 内部地址 client，用于上传/下载/列举等容器间操作
	presignClient *minio.Client // 浏览器可访问地址 client，专门用于生成预签名 URL
	bucket        string
	ttlSec        int
}

// New 初始化 MinIO 客户端并确保 bucket 存在。
// presignEndpoint: 浏览器可访问的 MinIO 地址（如 "localhost:9000"）。
// 在 Docker 中 endpoint 是 "minio:9000"（容器内），presignEndpoint 是 "localhost:9000"（浏览器用）。
// 空字符串表示两者相同（本地开发时不需要区分）。
//
// 预签名 URL 必须用浏览器最终访问时的 host 来签名（AWS SigV4 签名包含 Host），
// 所以不能先用内部地址签好名再替换 host 字符串——那样签名和 host 不匹配，
// MinIO 校验时会报 SignatureDoesNotMatch。因此这里为预签名单独建一个用外部
// 地址初始化的 client。
func New(endpoint, accessKey, secretKey, bucket string, useSSL bool, ttlSec int, presignEndpoint string) (*MinIOStorage, error) {
	client, err := minio.New(endpoint, &minio.Options{
		Creds:  credentials.NewStaticV4(accessKey, secretKey, ""),
		Secure: useSSL,
	})
	if err != nil {
		return nil, err
	}

	ctx := context.Background()
	exists, err := client.BucketExists(ctx, bucket)
	if err != nil {
		return nil, err
	}
	if !exists {
		if err := client.MakeBucket(ctx, bucket, minio.MakeBucketOptions{}); err != nil {
			return nil, err
		}
	}

	if ttlSec <= 0 {
		ttlSec = 3600
	}

	presignClient := client
	if presignEndpoint != "" && presignEndpoint != endpoint {
		presignClient, err = minio.New(presignEndpoint, &minio.Options{
			Creds:  credentials.NewStaticV4(accessKey, secretKey, ""),
			Secure: useSSL,
		})
		if err != nil {
			return nil, err
		}
	}

	return &MinIOStorage{
		client:        client,
		presignClient: presignClient,
		bucket:        bucket,
		ttlSec:        ttlSec,
	}, nil
}

func (s *MinIOStorage) PutFile(key string, filePath string) error {
	_, err := s.client.FPutObject(context.Background(), s.bucket, key, filePath, minio.PutObjectOptions{})
	return err
}

func (s *MinIOStorage) PutObject(key string, reader io.Reader, size int64, contentType string) error {
	_, err := s.client.PutObject(context.Background(), s.bucket, key, reader, size,
		minio.PutObjectOptions{ContentType: contentType})
	return err
}

func (s *MinIOStorage) GetObject(key string) (io.ReadCloser, error) {
	obj, err := s.client.GetObject(context.Background(), s.bucket, key, minio.GetObjectOptions{})
	if err != nil {
		return nil, err
	}
	return obj, nil
}

func (s *MinIOStorage) PreSign(key string, ttlSec int) (string, error) {
	if ttlSec <= 0 {
		ttlSec = s.ttlSec
	}
	u, err := s.presignClient.PresignedGetObject(context.Background(), s.bucket, key,
		time.Duration(ttlSec)*time.Second, url.Values{})
	if err != nil {
		return "", err
	}
	return u.String(), nil
}

func (s *MinIOStorage) Delete(key string) error {
	return s.client.RemoveObject(context.Background(), s.bucket, key, minio.RemoveObjectOptions{})
}

func (s *MinIOStorage) IsExist(key string) (bool, error) {
	_, err := s.client.StatObject(context.Background(), s.bucket, key, minio.StatObjectOptions{})
	if err != nil {
		if minio.ToErrorResponse(err).Code == "NoSuchKey" {
			return false, nil
		}
		return false, err
	}
	return true, nil
}

func (s *MinIOStorage) ListObjects(prefix string) ([]string, error) {
	var keys []string
	for obj := range s.client.ListObjects(context.Background(), s.bucket,
		minio.ListObjectsOptions{Prefix: prefix, Recursive: true}) {
		if obj.Err != nil {
			return nil, obj.Err
		}
		keys = append(keys, obj.Key)
	}
	return keys, nil
}
