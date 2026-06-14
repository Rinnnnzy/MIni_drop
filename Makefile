.PHONY: demo up down clean

demo:
	@echo "Starting Mini-Drop..."
	docker compose up -d
	@echo "Waiting for services to be healthy..."
	@until curl -sf http://localhost:8191/healthz > /dev/null 2>&1; do \
		echo "  waiting for apiserver..."; sleep 3; done
	@echo "Creating demo task..."
	@curl -sf -X POST http://localhost:8191/api/v1/tasks \
		-H "Content-Type: application/json" \
		-d '{"name":"demo","type":0,"profilerType":0,"targetIP":"127.0.0.1","pid":1,"duration":10,"hz":99}' \
		| python3 -m json.tool || true
	@echo ""
	@echo "Mini-Drop is running!"
	@echo "  Web UI:        http://localhost"
	@echo "  apiserver:     http://localhost:8191"
	@echo "  MinIO console: http://localhost:9001 (drop/dropdrop)"

up:
	docker compose up -d

down:
	docker compose down

clean:
	docker compose down -v --rmi local
