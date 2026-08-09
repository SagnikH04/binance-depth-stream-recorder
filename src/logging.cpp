#include "logging.hpp"

void print_summary(const Counters& c) {
    std::cerr << "\n=== Capture Summary ===\n"
              << "messages_received:     " << c.messages_received << "\n"
              << "market_rows_written:   " << c.market_rows_written << "\n"
              << "orderbook_rows_written:" << c.orderbook_rows_written << "\n"
              << "trades_seen:           " << c.trades_seen << "\n"
              << "depth_diffs_seen:      " << c.depth_diffs_seen << "\n"
              << "depth5_seen:           " << c.depth5_seen << "\n"
              << "parse_errors:          " << c.parse_errors << "\n"
              << "csv_errors:            " << c.csv_errors << "\n"
              << "reconnects:            " << c.reconnects << "\n"
              << "gap_events:            " << c.gap_events << "\n"
              << "stale_diffs_dropped:   " << c.stale_diffs_dropped << "\n"
              << "=======================\n";
}

void log_error(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << "\n";
}

void log_info(const std::string& msg) {
    std::cerr << "[INFO] " << msg << "\n";
}
