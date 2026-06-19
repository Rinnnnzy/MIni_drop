import os

from minio import Minio
from minio.error import S3Error

from error import AnalysisError, ERR_STORAGE


class StorageClient:
    def __init__(self):
        endpoint   = os.environ["MINIO_ENDPOINT"]
        access_key = os.environ["MINIO_ACCESS_KEY"]
        secret_key = os.environ["MINIO_SECRET_KEY"]
        secure     = os.environ.get("MINIO_SECURE", "false").lower() == "true"
        self.bucket = os.environ["MINIO_BUCKET"]
        self.client = Minio(
            endpoint,
            access_key=access_key,
            secret_key=secret_key,
            secure=secure,
        )

    def download(self, cos_key: str, local_path: str):
        try:
            self.client.fget_object(self.bucket, cos_key, local_path)
        except S3Error as e:
            raise AnalysisError(ERR_STORAGE, f"download '{cos_key}' failed: {e}")

    def upload(self, local_path: str, cos_key: str):
        content_type = "image/svg+xml" if cos_key.endswith(".svg") else "application/octet-stream"
        try:
            self.client.fput_object(self.bucket, cos_key, local_path, content_type=content_type)
        except S3Error as e:
            raise AnalysisError(ERR_STORAGE, f"upload '{cos_key}' failed: {e}")
