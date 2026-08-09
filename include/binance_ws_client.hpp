#pragma once
#include "event_processor.hpp"
#include "app_config.hpp"
#include <atomic>
#include <chrono>
#include <string>

class BinanceWsClient {
public:
    BinanceWsClient(const AppConfig& config, EventProcessor& processor,
                    std::atomic<bool>& stop_flag);

    void run();

private:
    const AppConfig& config_;
    EventProcessor& processor_;
    std::atomic<bool>& stop_flag_;

    uint32_t conn_epoch_ = 0;
    uint64_t conn_seq_ = 0;

    std::string build_host() const;
    std::string build_port() const;
    std::string build_target() const;

    void connect_and_read(std::chrono::steady_clock::time_point program_start);
    int backoff_seconds(int attempt) const;
};
