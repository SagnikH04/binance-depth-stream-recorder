#pragma once
#include "csv_writer.hpp"
#include "json_parsers.hpp"
#include "logging.hpp"
#include "market_event.hpp"
#include "order_book.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

class EventProcessor {
public:
    EventProcessor(Venue venue, const std::vector<std::string>& symbols,
                   const std::string& output_dir, const std::string& date_str,
                   bool replay_mode = false);

    void process_raw_ws_message(const std::string& raw, const ReceiveTimestamp& recv_ts,
                                uint32_t shard_id, uint32_t conn_epoch, uint64_t conn_seq);

    void process_market_event_from_replay(const MarketEvent& event);

    void mark_all_books_stale_on_reconnect();
    void record_reconnect();
    void flush();
    void print_counters() const;
    const Counters& counters() const { return counters_; }

private:
    Venue venue_;
    std::vector<std::string> symbols_;
    std::string output_dir_;

    std::unique_ptr<CsvWriter> market_writer_;
    std::map<std::string, std::unique_ptr<CsvWriter>> orderbook_writers_;
    std::map<std::string, OrderBook> books_;
    std::map<std::string, int32_t> instrument_ids_;
    std::map<std::string, uint64_t> seq_nos_;

    Counters counters_;

    void write_market_row(const MarketEvent& event);
    void write_orderbook_row(const std::string& symbol, const ReceiveTimestamp& ts,
                             char type_char, char side_char);
    void process_event_logic(const MarketEvent& event, const nlohmann::json& data,
                             StreamKind kind);
    char determine_side(const DepthUpdate& update);
};
