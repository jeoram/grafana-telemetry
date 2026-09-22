# Build stage
FROM alpine:3.19 AS builder

RUN apk add --no-cache g++ make libstdc++

WORKDIR /app

COPY src/ ./src/

# Compile multi-threaded C++ engine with optimization
RUN g++ -O3 -std=c++17 src/main.cpp -pthread -o trading_engine

# Runtime stage
FROM alpine:3.19

RUN apk add --no-cache libstdc++

WORKDIR /app

COPY --from=builder /app/trading_engine /app/trading_engine

EXPOSE 8080

CMD ["/app/trading_engine"]
