#ifndef METRICS_EXPORTER_HPP
#define METRICS_EXPORTER_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>

class MetricsExporter {
public:
    MetricsExporter() : 
        orders_limit_(0), 
        orders_market_(0),
        bid_ask_spread_(0.05),
        order_book_depth_(1500),
        active_threads_(4),
        cpu_usage_percent_(12.5),
        memory_bytes_(42000000) 
    {
        // Define histogram buckets in microseconds
        buckets_ = {10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2500.0, 5000.0, 10000.0};
        bucket_counts_.resize(buckets_.size(), 0);
        latency_sum_us_ = 0.0;
        latency_total_count_ = 0;
    }

    // Increments
    void increment_limit_orders(uint64_t count = 1) {
        orders_limit_.fetch_add(count, std::memory_order_relaxed);
    }

    void increment_market_orders(uint64_t count = 1) {
        orders_market_.fetch_add(count, std::memory_order_relaxed);
    }

    // Gauges
    void set_bid_ask_spread(double val) {
        std::lock_guard<std::mutex> lock(gauge_mutex_);
        bid_ask_spread_ = val;
    }

    void set_order_book_depth(int64_t val) {
        order_book_depth_.store(val, std::memory_order_relaxed);
    }

    void set_active_threads(int64_t val) {
        active_threads_.store(val, std::memory_order_relaxed);
    }

    void set_cpu_usage(double val) {
        std::lock_guard<std::mutex> lock(gauge_mutex_);
        cpu_usage_percent_ = val;
    }

    void set_memory_bytes(int64_t val) {
        memory_bytes_.store(val, std::memory_order_relaxed);
    }

    // Record Latency (in microseconds)
    void observe_latency(double latency_us) {
        std::lock_guard<std::mutex> lock(histogram_mutex_);
        latency_sum_us_ += latency_us;
        latency_total_count_++;

        for (size_t i = 0; i < buckets_.size(); ++i) {
            if (latency_us <= buckets_[i]) {
                bucket_counts_[i]++;
            }
        }
    }

    // Format metrics into Prometheus exposition text format
    std::string generate_prometheus_text() {
        std::ostringstream ss;

        // Counters
        ss << "# HELP trading_orders_processed_total Total number of orders processed by type\n";
        ss << "# TYPE trading_orders_processed_total counter\n";
        ss << "trading_orders_processed_total{type=\"limit\"} " << orders_limit_.load() << "\n";
        ss << "trading_orders_processed_total{type=\"market\"} " << orders_market_.load() << "\n\n";

        // Gauges
        double current_spread = 0.0;
        double current_cpu = 0.0;
        {
            std::lock_guard<std::mutex> lock(gauge_mutex_);
            current_spread = bid_ask_spread_;
            current_cpu = cpu_usage_percent_;
        }

        ss << "# HELP trading_bid_ask_spread_dollars Current bid-ask spread in USD\n";
        ss << "# TYPE trading_bid_ask_spread_dollars gauge\n";
        ss << "trading_bid_ask_spread_dollars " << current_spread << "\n\n";

        ss << "# HELP trading_order_book_depth Current total depth of order book\n";
        ss << "# TYPE trading_order_book_depth gauge\n";
        ss << "trading_order_book_depth " << order_book_depth_.load() << "\n\n";

        ss << "# HELP trading_active_threads Active worker threads in matching engine\n";
        ss << "# TYPE trading_active_threads gauge\n";
        ss << "trading_active_threads " << active_threads_.load() << "\n\n";

        ss << "# HELP trading_engine_cpu_percent CPU usage percentage of trading engine\n";
        ss << "# TYPE trading_engine_cpu_percent gauge\n";
        ss << "trading_engine_cpu_percent " << current_cpu << "\n\n";

        ss << "# HELP trading_engine_memory_bytes RAM memory usage of trading engine in bytes\n";
        ss << "# TYPE trading_engine_memory_bytes gauge\n";
        ss << "trading_engine_memory_bytes " << memory_bytes_.load() << "\n\n";

        // Histogram
        ss << "# HELP trading_order_latency_microseconds Order matching latency in microseconds\n";
        ss << "# TYPE trading_order_latency_microseconds histogram\n";

        {
            std::lock_guard<std::mutex> lock(histogram_mutex_);
            uint64_t cumulative = 0;
            for (size_t i = 0; i < buckets_.size(); ++i) {
                cumulative += bucket_counts_[i];
                ss << "trading_order_latency_microseconds_bucket{le=\"" << buckets_[i] << "\"} " << cumulative << "\n";
            }
            ss << "trading_order_latency_microseconds_bucket{le=\"+Inf\"} " << latency_total_count_ << "\n";
            ss << "trading_order_latency_microseconds_sum " << latency_sum_us_ << "\n";
            ss << "trading_order_latency_microseconds_count " << latency_total_count_ << "\n";
        }

        return ss.str();
    }

private:
    std::atomic<uint64_t> orders_limit_;
    std::atomic<uint64_t> orders_market_;
    
    std::mutex gauge_mutex_;
    double bid_ask_spread_;
    double cpu_usage_percent_;

    std::atomic<int64_t> order_book_depth_;
    std::atomic<int64_t> active_threads_;
    std::atomic<int64_t> memory_bytes_;

    std::mutex histogram_mutex_;
    std::vector<double> buckets_;
    std::vector<uint64_t> bucket_counts_;
    double latency_sum_us_;
    uint64_t latency_total_count_;
};

#endif // METRICS_EXPORTER_HPP
