#include "replay.hpp"
#include "csv_reader.hpp"
#include "event_processor.hpp"
#include "logging.hpp"
#include "market_event.hpp"
#include "time_utils.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

static const std::string EXPECTED_HEADER =
    "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json";

int run_replay(const std::string& replay_file, const std::string& output_dir) {
    log_info("Replay mode: reading " + replay_file);

    CsvReader reader(replay_file);

    // Read and validate header
    std::vector<std::string> header;
    if (!reader.read_row(header)) {
        log_error("Empty CSV file");
        return 1;
    }

    // Reconstruct header string for validation
    std::string header_str;
    for (size_t i = 0; i < header.size(); i++) {
        if (i > 0) header_str += ',';
        header_str += header[i];
    }
    if (header_str != EXPECTED_HEADER) {
        log_error("Header mismatch. Expected:\n  " + EXPECTED_HEADER +
                  "\nGot:\n  " + header_str);
        return 1;
    }

    // Collect symbols preserving first-seen order
    std::vector<std::string> symbols;
    std::set<std::string> seen_symbols;
    Venue venue = Venue::Spot;
    bool venue_set = false;

    std::vector<MarketEvent> events;

    std::vector<std::string> row;
    while (reader.read_row(row)) {
        if (row.size() != 9) {
            log_error("Row " + std::to_string(reader.row_number()) +
                      ": expected 9 columns, got " + std::to_string(row.size()));
            return 1;
        }

        MarketEvent event;
        event.recv_ts.tsec = std::stoll(row[0]);
        event.recv_ts.tnsec = std::stoi(row[1]);
        event.venue = parse_venue(row[2]);
        event.stream_kind = parse_stream_kind(row[3]);
        event.shard_id = static_cast<uint32_t>(std::stoul(row[4]));
        event.conn_epoch = static_cast<uint32_t>(std::stoul(row[5]));
        event.conn_seq = std::stoull(row[6]);
        event.symbol = row[7];
        event.payload_json = row[8];

        if (seen_symbols.find(event.symbol) == seen_symbols.end()) {
            symbols.push_back(event.symbol);
            seen_symbols.insert(event.symbol);
        }
        if (!venue_set) {
            venue = event.venue;
            venue_set = true;
        }

        events.push_back(std::move(event));
    }

    if (events.empty()) {
        log_info("No data rows in replay file");
        return 0;
    }

    std::string date_str = utc_date_yyyy_mm_dd();

    EventProcessor processor(venue, symbols, output_dir, date_str, true);

    for (const auto& event : events) {
        processor.process_market_event_from_replay(event);
    }

    processor.flush();
    processor.print_counters();

    log_info("Replay complete");
    return 0;
}
