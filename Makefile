.PHONY: up down logs restart clean test-metrics

up:
	docker compose up --build -d

down:
	docker compose down

logs:
	docker compose logs -f

logs-cpp:
	docker compose logs -f cpp-engine

restart:
	docker compose restart

test-metrics:
	curl -s http://localhost:8080/metrics | head -n 30

clean:
	docker compose down -v --remove-orphans
