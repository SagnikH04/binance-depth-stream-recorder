#include "event_processor.hpp"
#include <nlohmann/json.hpp>

static const std::vector<std::string> kMarketCsvHeader = {
    "recv_tsec", "recv_tnsec", "venue", "stream_kind", "shard_id",
    "conn_epoch", "conn_seq", "symbol", "payload_json"
};

static const std::vector<std::string> kOrderBookCsvHeader = {
    "tsec", "tnsec", "seqNo", "id", "type", "side",
    "bid0", "bid1", "bid2", "bid3", "bid4",
    "bid_size0", "bid_size1", "bid_size2", "bid_size3", "bid_size4",
    "ask0", "ask1", "ask2", "ask3", "ask4",
    "ask_size0", "ask_size1", "ask_size2", "ask_size3", "ask_size4"
};

EventProcessor::EventProcessor(Venue venue, const std::vector<std::string>& symbols,
                               const std::string& output_dir, const std::string& date_str,
                               bool replay_mode)
    : venue_(venue), symbols_(symbols), output_dir_(output_dir) {

    // Instrument IDs stay stable for the order supplied on the CLI.
    for (size_t i = 0; i < symbols.size(); i++) {
        instrument_ids_[symbols[i]] = static_cast<int32_t>(i + 1);
        seq_nos_[symbols[i]] = 0;
    }

    // Market-data capture is skipped during replay.
    if (!replay_mode) {
        std::string venue_str = venue_to_string(venue);
        std::string symbol_part;
        if (symbols.size() == 1) {
            symbol_part = symbols[0];
        } else {
            symbol_part = "MULTI";
        }

        std::string market_file = output_dir + "/market_data_" + venue_str + "_" +
                                  symbol_part + "_" + date_str + ".csv";
        market_writer_ = std::make_unique<CsvWriter>(market_file);
        market_writer_->write_header(kMarketCsvHeader);
    }

    // Each symbol gets its own order-book CSV.
    std::string venue_str = venue_to_string(venue);
    for (const auto& sym : symbols) {
        std::string suffix = replay_mode ? "_replay_orderbook.csv" : "_orderbook.csv";
        std::string ob_file = output_dir + "/market_data_" + venue_str + "_" +
                              sym + "_" + date_str + suffix;
        orderbook_writers_[sym] = std::make_unique<CsvWriter>(ob_file);
        orderbook_writers_[sym]->write_header(kOrderBookCsvHeader);
        books_[sym] = OrderBook();
    }
}

void EventProcessor::write_market_row(const MarketEvent& event) {
    if (!market_writer_) return;
    std::vector<std::string> row = {
        std::to_string(event.recv_ts.tsec),
        std::to_string(event.recv_ts.tnsec),
        venue_to_string(event.venue),
        stream_kind_to_string(event.stream_kind),
        std::to_string(event.shard_id),
        std::to_string(event.conn_epoch),
        std::to_string(event.conn_seq),
        event.symbol,
        event.payload_json
    };
    market_writer_->write_row(row);
    counters_.market_rows_written++;
}

void EventProcessor::write_orderbook_row(const std::string& symbol, const ReceiveTimestamp& ts,
                                         char type_char, char side_char) {
    auto it = books_.find(symbol);
    if (it == books_.end()) return;

    auto snap = it->second.top5();
    seq_nos_[symbol]++;

    std::vector<std::string> row;
    row.reserve(26);

    row.push_back(std::to_string(ts.tsec));
    row.push_back(std::to_string(ts.tnsec));
    row.push_back(std::to_string(seq_nos_[symbol]));
    row.push_back(std::to_string(instrument_ids_[symbol]));
    row.push_back(std::string(1, type_char));
    row.push_back(std::string(1, side_char));

    for (int i = 0; i < 5; i++) row.push_back(std::to_string(snap.bid_prices[i]));
    for (int i = 0; i < 5; i++) row.push_back(std::to_string(snap.bid_sizes[i]));
    for (int i = 0; i < 5; i++) row.push_back(std::to_string(snap.ask_prices[i]));
    for (int i = 0; i < 5; i++) row.push_back(std::to_string(snap.ask_sizes[i]));

    auto wit = orderbook_writers_.find(symbol);
    if (wit != orderbook_writers_.end()) {
        wit->second->write_row(row);
        counters_.orderbook_rows_written++;
    }
}

char EventProcessor::determine_side(const DepthUpdate& update) {
    bool has_bids = !update.bids.empty();
    bool has_asks = !update.asks.empty();
    if (has_bids && has_asks) return 'N';
    if (has_bids) return 'B';
    if (has_asks) return 'S';
    return 'N';
}

void EventProcessor::process_event_logic(const MarketEvent& event, const nlohmann::json& data,
                                         StreamKind kind) {
    const std::string& symbol = event.symbol;

    // Replay can introduce symbols that were not in the live CLI list.
    if (books_.find(symbol) == books_.end()) {
        if (instrument_ids_.find(symbol) == instrument_ids_.end()) {
            instrument_ids_[symbol] = static_cast<int32_t>(instrument_ids_.size() + 1);
            seq_nos_[symbol] = 0;
            books_[symbol] = OrderBook();
        }
    }

    switch (kind) {
        case StreamKind::Trade: {
            parse_trade(data);
            counters_.trades_seen++;
            break;
        }
        case StreamKind::Depth5: {
            counters_.depth5_seen++;
            auto snapshot = parse_depth5_snapshot(data, symbol);
            if (snapshot.symbol.empty()) snapshot.symbol = symbol;
            books_[symbol].apply_depth5(snapshot);
            write_orderbook_row(symbol, event.recv_ts, 'P', 'N');
            break;
        }
        case StreamKind::DepthDiff: {
            counters_.depth_diffs_seen++;
            auto update = parse_depth_update(data, symbol);
            if (update.symbol.empty()) update.symbol = symbol;
            auto result = books_[symbol].apply_depth_update(update);
            switch (result) {
                case ApplyResult::Applied: {
                    char side = determine_side(update);
                    write_orderbook_row(symbol, event.recv_ts, 'D', side);
                    break;
                }
                case ApplyResult::SkippedStaleOrOld:
                    counters_.stale_diffs_dropped++;
                    break;
                case ApplyResult::GapDetected:
                    counters_.gap_events++;
                    books_[symbol].clear_and_mark_stale();
                    break;
            }
            break;
        }
    }
}

void EventProcessor::process_raw_ws_message(const std::string& raw, const ReceiveTimestamp& recv_ts,
                                            uint32_t shard_id, uint32_t conn_epoch, uint64_t conn_seq) {
    counters_.messages_received++;

    try {
        auto parsed = parse_combined_message(raw);

        MarketEvent event;
        event.recv_ts = recv_ts;
        event.venue = venue_;
        event.stream_kind = parsed.stream_kind;
        event.shard_id = shard_id;
        event.conn_epoch = conn_epoch;
        event.conn_seq = conn_seq;
        event.symbol = parsed.symbol_upper;
        event.payload_json = parsed.payload_json_minified;

        write_market_row(event);
        process_event_logic(event, parsed.data, parsed.stream_kind);

    } catch (const std::exception& e) {
        counters_.parse_errors++;
        log_error(std::string("Parse error: ") + e.what());
    }
}

void EventProcessor::process_market_event_from_replay(const MarketEvent& event) {
    counters_.messages_received++;

    try {
        auto data = nlohmann::json::parse(event.payload_json);
        process_event_logic(event, data, event.stream_kind);
    } catch (const std::exception& e) {
        counters_.parse_errors++;
        log_error(std::string("Replay parse error: ") + e.what());
    }
}

void EventProcessor::mark_all_books_stale_on_reconnect() {
    for (auto& [sym, book] : books_) {
        book.clear_and_mark_stale();
    }
}

void EventProcessor::record_reconnect() {
    counters_.reconnects++;
}

void EventProcessor::flush() {
    if (market_writer_) market_writer_->flush();
    for (auto& [sym, writer] : orderbook_writers_) {
        writer->flush();
    }
}

void EventProcessor::print_counters() const {
    print_summary(counters_);
}
