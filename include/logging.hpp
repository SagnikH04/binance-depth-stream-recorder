#pragma once
#include <cstdint>
#include <iostream>
#include <string>

struct Counters {
    uint64_t messages_received = 0;
    uint64_t market_rows_written = 0;
    uint64_t orderbook_rows_written = 0;
    uint64_t trades_seen = 0;
    uint64_t depth_diffs_seen = 0;
    uint64_t depth5_seen = 0;
    uint64_t parse_errors = 0;
    uint64_t csv_errors = 0;
    uint64_t reconnects = 0;
    uint64_t gap_events = 0;
    uint64_t stale_diffs_dropped = 0;
};

void print_summary(const Counters& c);
void log_error(const std::string& msg);
void log_info(const std::string& msg);
