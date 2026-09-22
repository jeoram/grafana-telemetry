#include "metrics_exporter.hpp"
#include "trading_engine.hpp"

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

std::atomic<bool> g_server_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[C++ Telemetry Engine] Shutdown signal received (" << signal << ")...\n";
        g_server_running.store(false);
    }
}

void handle_client(int client_fd, MetricsExporter& exporter) {
    char buffer[2048] = {0};
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        std::string request(buffer);

        if (request.find("GET /metrics") != std::string::npos) {
            std::string metrics_data = exporter.generate_prometheus_text();
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                "Content-Length: " + std::to_string(metrics_data.length()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + metrics_data;
            write(client_fd, response.c_str(), response.length());
        } else if (request.find("GET /health") != std::string::npos || request.find("GET / ") != std::string::npos) {
            std::string body = "{\"status\":\"UP\",\"service\":\"C++ Trading Telemetry Engine\"}\n";
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + body;
            write(client_fd, response.c_str(), response.length());
        } else {
            std::string body = "404 Not Found\n";
            std::string response = 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + body;
            write(client_fd, response.c_str(), response.length());
        }
    }
    close(client_fd);
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=======================================================\n";
    std::cout << "  C++ High-Performance Trading & Telemetry Engine v1.0 \n";
    std::cout << "=======================================================\n";

    MetricsExporter exporter;
    TradingEngine engine(exporter, 4); // 4 simulated worker threads

    std::cout << "[C++ Engine] Starting multi-threaded order book simulation...\n";
    engine.start();

    int port = 8080;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "[ERROR] Socket creation failed!\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "[ERROR] Bind failed on port " << port << "\n";
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "[ERROR] Listen failed!\n";
        return 1;
    }

    std::cout << "[C++ HTTP Server] Listening on http://0.0.0.0:" << port << "/metrics\n";

    // Non-blocking socket timeout for graceful shutdown loop
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    while (g_server_running.load()) {
        sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd >= 0) {
            std::thread(handle_client, client_fd, std::ref(exporter)).detach();
        }
    }

    std::cout << "[C++ Engine] Stopping trading engine workers...\n";
    engine.stop();

    close(server_fd);
    std::cout << "[C++ Engine] Shutdown complete. Goodbye!\n";
    return 0;
}
