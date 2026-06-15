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
	client *minio.Client
	bucket string
	ttlSec int // 预签名 URL 默认有效期
}

// New 初始化 MinIO 客户端并确保 bucket 存在。
func New(endpoint, accessKey, secretKey, bucket string, useSSL bool, ttlSec int) (*MinIOStorage, error) {
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

	return &MinIOStorage{client: client, bucket: bucket, ttlSec: ttlSec}, nil
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
	u, err := s.client.PresignedGetObject(context.Background(), s.bucket, key,
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
