#ifndef TRADING_ENGINE_HPP
#define TRADING_ENGINE_HPP

#include "metrics_exporter.hpp"
#include <thread>
#include <atomic>
#include <random>
#include <chrono>
#include <cmath>
#include <vector>

class TradingEngine {
public:
    TradingEngine(MetricsExporter& exporter, int num_threads = 4)
        : exporter_(exporter), running_(false), num_threads_(num_threads) {}

    ~TradingEngine() {
        stop();
    }

    void start() {
        if (running_.load()) return;
        running_.store(true);

        exporter_.set_active_threads(num_threads_);

        // Spawn worker threads for order generation & matching simulation
        for (int i = 0; i < num_threads_; ++i) {
            workers_.emplace_back(&TradingEngine::worker_loop, this, i);
        }

        // Spawn a monitoring thread for simulated system metrics (CPU, Memory, Volatility)
        monitor_thread_ = std::thread(&TradingEngine::monitor_loop, this);
    }

    void stop() {
        if (!running_.load()) return;
        running_.store(false);

        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();

        if (monitor_thread_.joinable()) monitor_thread_.join();
    }

private:
    void worker_loop(int thread_id) {
        std::mt19937 rng(std::random_device{}() + thread_id);
        std::uniform_int_distribution<int> order_type_dist(0, 10); // 0-7 limit, 8-10 market
        std::normal_distribution<double> latency_dist(120.0, 45.0); // mean 120us, stddev 45us
        std::uniform_int_distribution<int> sleep_dist(5, 25); // sleep between orders in ms

        while (running_.load()) {
            auto start_time = std::chrono::high_resolution_clock::now();

            // Simulate order processing logic
            int order_type = order_type_dist(rng);
            if (order_type <= 7) {
                exporter_.increment_limit_orders(1);
            } else {
                exporter_.increment_market_orders(1);
            }

            // Simulate execution latency calculation with occasional spikes
            double latency = std::max(5.0, latency_dist(rng));
            if (rng() % 100 < 3) { // 3% probability of latency spike
                latency += 1500.0 + (rng() % 3000);
            }

            // Record latency
            exporter_.observe_latency(latency);

            // Small sleep to simulate realistic order arrival frequency per thread
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(rng)));
        }
    }

    void monitor_loop() {
        std::mt19937 rng(1337);
        std::normal_distribution<double> spread_dist(0.08, 0.03);
        std::normal_distribution<double> cpu_dist(18.5, 5.0);
        std::uniform_int_distribution<int64_t> depth_dist(1200, 2800);

        double base_mem = 45.0 * 1024 * 1024; // 45 MB base

        while (running_.load()) {
            // Dynamic Bid-Ask spread
            double current_spread = std::abs(spread_dist(rng));
            if (current_spread < 0.01) current_spread = 0.01;
            exporter_.set_bid_ask_spread(current_spread);

            // Dynamic Order Book Depth
            exporter_.set_order_book_depth(depth_dist(rng));

            // Dynamic CPU %
            double cpu_val = std::clamp(cpu_dist(rng), 5.0, 95.0);
            exporter_.set_cpu_usage(cpu_val);

            // Dynamic Memory usage
            double mem_jitter = (rng() % 5000000) - 2500000;
            exporter_.set_memory_bytes(static_cast<int64_t>(base_mem + mem_jitter));

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    MetricsExporter& exporter_;
    std::atomic<bool> running_;
    int num_threads_;
    std::vector<std::thread> workers_;
    std::thread monitor_thread_;
};

#endif // TRADING_ENGINE_HPP
