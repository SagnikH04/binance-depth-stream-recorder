#undef NDEBUG

#include "csv_reader.hpp"
#include "csv_writer.hpp"
#include "decimal_scaler.hpp"
#include "event_processor.hpp"
#include "json_parsers.hpp"
#include "order_book.hpp"
#include "replay.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        std::cout << "  TEST: " << #name << " ... "; \
        try { \
            test_##name(); \
            tests_passed++; \
            std::cout << "PASS\n"; \
        } catch (const std::exception& e) { \
            std::cout << "FAIL: " << e.what() << "\n"; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a != _b) { \
            throw std::runtime_error( \
                "Assertion failed at line " + std::to_string(__LINE__)); \
        } \
    } while(0)

#define ASSERT_THROWS(expr) \
    do { \
        bool _threw = false; \
        try { expr; } catch (...) { _threw = true; } \
        if (!_threw) { \
            throw std::runtime_error("Expected exception at line " + std::to_string(__LINE__)); \
        } \
    } while(0)

// ============= Decimal Scaler Tests =============

void test_decimal_1() {
    ASSERT_EQ(parse_scaled_decimal("1", PRICE_SCALE), 100000000LL);
}

void test_decimal_1_23() {
    ASSERT_EQ(parse_scaled_decimal("1.23", PRICE_SCALE), 123000000LL);
}

void test_decimal_tiny() {
    ASSERT_EQ(parse_scaled_decimal("0.00000001", PRICE_SCALE), 1LL);
}

void test_decimal_large() {
    ASSERT_EQ(parse_scaled_decimal("43123.45000000", PRICE_SCALE), 4312345000000LL);
}

void test_decimal_zero() {
    ASSERT_EQ(parse_scaled_decimal("0", PRICE_SCALE), 0LL);
}

void test_decimal_invalid_empty() {
    ASSERT_THROWS(parse_scaled_decimal("", PRICE_SCALE));
}

void test_decimal_invalid_dot() {
    ASSERT_THROWS(parse_scaled_decimal(".", PRICE_SCALE));
}

void test_decimal_invalid_abc() {
    ASSERT_THROWS(parse_scaled_decimal("abc", PRICE_SCALE));
}

void test_decimal_invalid_two_dots() {
    ASSERT_THROWS(parse_scaled_decimal("1.2.3", PRICE_SCALE));
}

// ============= CSV Escaping Tests =============

void test_csv_escape_plain() {
    ASSERT_EQ(CsvWriter::escape_field("abc"), std::string("abc"));
}

void test_csv_escape_comma() {
    ASSERT_EQ(CsvWriter::escape_field("a,b"), std::string("\"a,b\""));
}

void test_csv_escape_quote() {
    ASSERT_EQ(CsvWriter::escape_field("a\"b"), std::string("\"a\"\"b\""));
}

void test_csv_escape_newline() {
    ASSERT_EQ(CsvWriter::escape_field("a\nb"), std::string("\"a\nb\""));
}

void test_csv_escape_json() {
    std::string json = R"({"e":"trade","s":"BTCUSDT"})";
    std::string expected = R"("{""e"":""trade"",""s"":""BTCUSDT""}")";
    ASSERT_EQ(CsvWriter::escape_field(json), expected);
}

// ============= CSV Reader Tests =============

void test_csv_roundtrip() {
    std::string tmpfile = "/tmp/test_csv_roundtrip.csv";
    {
        CsvWriter writer(tmpfile);
        writer.write_row({"hello", "wor,ld", "quo\"te", R"({"a":"b"})"});
        writer.write_row({"simple", "data", "here", "ok"});
    }
    {
        CsvReader reader(tmpfile);
        std::vector<std::string> row;

        if (!reader.read_row(row)) throw std::runtime_error("read_row failed");
        ASSERT_EQ(row.size(), size_t(4));
        ASSERT_EQ(row[0], std::string("hello"));
        ASSERT_EQ(row[1], std::string("wor,ld"));
        ASSERT_EQ(row[2], std::string("quo\"te"));
        ASSERT_EQ(row[3], std::string(R"({"a":"b"})"));

        if (!reader.read_row(row)) throw std::runtime_error("read_row2 failed");
        ASSERT_EQ(row.size(), size_t(4));
        ASSERT_EQ(row[0], std::string("simple"));
    }
    std::remove(tmpfile.c_str());
}

void test_csv_reader_payload_json() {
    std::string tmpfile = "/tmp/test_csv_payload.csv";
    {
        CsvWriter writer(tmpfile);
        writer.write_header({"col1", "col2", "payload"});
        std::string payload = R"({"bids":[["100.5","2.0"],["99.0","1.0"]],"asks":[["101.0","3.0"]]})";
        writer.write_row({"123", "456", payload});
    }
    {
        CsvReader reader(tmpfile);
        std::vector<std::string> row;
        if (!reader.read_row(row)) throw std::runtime_error("header read failed");
        if (!reader.read_row(row)) throw std::runtime_error("data read failed");
        ASSERT_EQ(row.size(), size_t(3));
        ASSERT_EQ(row[0], std::string("123"));
        ASSERT_EQ(row[2], std::string(R"({"bids":[["100.5","2.0"],["99.0","1.0"]],"asks":[["101.0","3.0"]]})"));
    }
    std::remove(tmpfile.c_str());
}

void test_csv_reader_unterminated_quote() {
    std::string tmpfile = "/tmp/test_csv_untermin.csv";
    {
        std::ofstream f(tmpfile);
        f << "a,\"unterminated";
    }
    CsvReader reader(tmpfile);
    std::vector<std::string> row;
    ASSERT_THROWS(reader.read_row(row));
    std::remove(tmpfile.c_str());
}

// ============= Order Book Tests =============

void test_orderbook_bid_insert() {
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{100, 5}};
    snap.asks = {{101, 3}};
    book.apply_depth5(snap);

    auto top = book.top5();
    ASSERT_EQ(top.bid_prices[0], 100LL);
    ASSERT_EQ(top.bid_sizes[0], 5LL);
    ASSERT_EQ(top.ask_prices[0], 101LL);
    ASSERT_EQ(top.ask_sizes[0], 3LL);
}

void test_orderbook_remove_level() {
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{100, 5}, {99, 3}};
    snap.asks = {{101, 2}};
    snap.last_update_id = 10;
    book.apply_depth5(snap);

    DepthUpdate upd;
    upd.first_update_id = 11;
    upd.final_update_id = 11;
    upd.bids = {{100, 0}};  // remove
    book.apply_depth_update(upd);

    auto top = book.top5();
    ASSERT_EQ(top.bid_prices[0], 99LL);
    ASSERT_EQ(top.bid_sizes[0], 3LL);
    ASSERT_EQ(top.bid_prices[1], 0LL);
}

void test_orderbook_sort_descending_bids() {
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{50, 1}, {100, 2}, {75, 3}};
    snap.asks = {{101, 1}};
    book.apply_depth5(snap);

    auto top = book.top5();
    ASSERT_EQ(top.bid_prices[0], 100LL);
    ASSERT_EQ(top.bid_prices[1], 75LL);
    ASSERT_EQ(top.bid_prices[2], 50LL);
}

void test_orderbook_sort_ascending_asks() {
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{99, 1}};
    snap.asks = {{105, 1}, {101, 2}, {103, 3}};
    book.apply_depth5(snap);

    auto top = book.top5();
    ASSERT_EQ(top.ask_prices[0], 101LL);
    ASSERT_EQ(top.ask_prices[1], 103LL);
    ASSERT_EQ(top.ask_prices[2], 105LL);
}

void test_orderbook_missing_levels_zero() {
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{100, 5}};
    snap.asks = {{101, 3}};
    book.apply_depth5(snap);

    auto top = book.top5();
    ASSERT_EQ(top.bid_prices[1], 0LL);
    ASSERT_EQ(top.bid_sizes[1], 0LL);
    ASSERT_EQ(top.ask_prices[1], 0LL);
    ASSERT_EQ(top.ask_sizes[1], 0LL);
}

void test_depth5_replaces_book() {
    OrderBook book;
    Depth5Snapshot snap1;
    snap1.bids = {{100, 5}, {99, 4}, {98, 3}, {97, 2}, {96, 1}};
    snap1.asks = {{101, 5}, {102, 4}, {103, 3}, {104, 2}, {105, 1}};
    snap1.last_update_id = 100;
    book.apply_depth5(snap1);

    Depth5Snapshot snap2;
    snap2.bids = {{200, 10}, {199, 9}, {198, 8}, {197, 7}, {196, 6}};
    snap2.asks = {{201, 10}, {202, 9}, {203, 8}, {204, 7}, {205, 6}};
    snap2.last_update_id = 200;
    book.apply_depth5(snap2);

    auto top = book.top5();
    ASSERT_EQ(top.bid_prices[0], 200LL);
    ASSERT_EQ(top.bid_sizes[0], 10LL);
    ASSERT_EQ(top.ask_prices[0], 201LL);
    ASSERT_EQ(top.ask_sizes[0], 10LL);
}

// ============= Order Book Continuity Tests (Issue 1) =============

void test_continuity_spot_stale_diff() {
    // depth5 lastUpdateId=100, diff U=99 u=100 -> SkippedStaleOrOld
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{100, 5}};
    snap.asks = {{101, 3}};
    snap.last_update_id = 100;
    book.apply_depth5(snap);

    DepthUpdate upd;
    upd.first_update_id = 99;
    upd.final_update_id = 100;
    upd.bids = {{99, 1}};
    ASSERT_EQ(book.apply_depth_update(upd), ApplyResult::SkippedStaleOrOld);
}

void test_continuity_spot_applied() {
    // depth5 lastUpdateId=100, diff U=99 u=101 -> Applied
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{100, 5}};
    snap.asks = {{101, 3}};
    snap.last_update_id = 100;
    book.apply_depth5(snap);

    DepthUpdate upd;
    upd.first_update_id = 99;
    upd.final_update_id = 101;
    upd.bids = {{99, 2}};
    ASSERT_EQ(book.apply_depth_update(upd), ApplyResult::Applied);
}

void test_continuity_spot_gap() {
    // depth5 lastUpdateId=100, diff U=102 u=105 -> GapDetected
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{100, 5}};
    snap.asks = {{101, 3}};
    snap.last_update_id = 100;
    book.apply_depth5(snap);

    DepthUpdate upd;
    upd.first_update_id = 102;
    upd.final_update_id = 105;
    upd.bids = {{99, 2}};
    ASSERT_EQ(book.apply_depth_update(upd), ApplyResult::GapDetected);
}

void test_continuity_futures_applied() {
    // depth5 lastUpdateId=100, futures diff pu=100 u=101 -> Applied
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{100, 5}};
    snap.asks = {{101, 3}};
    snap.last_update_id = 100;
    book.apply_depth5(snap);

    DepthUpdate upd;
    upd.previous_final_update_id = 100;
    upd.final_update_id = 101;
    upd.bids = {{99, 2}};
    ASSERT_EQ(book.apply_depth_update(upd), ApplyResult::Applied);
}

void test_continuity_futures_gap() {
    // depth5 lastUpdateId=100, futures diff pu=99 u=101 -> GapDetected
    OrderBook book;
    Depth5Snapshot snap;
    snap.bids = {{100, 5}};
    snap.asks = {{101, 3}};
    snap.last_update_id = 100;
    book.apply_depth5(snap);

    DepthUpdate upd;
    upd.previous_final_update_id = 99;
    upd.final_update_id = 101;
    upd.bids = {{99, 2}};
    ASSERT_EQ(book.apply_depth_update(upd), ApplyResult::GapDetected);
}

// ============= JSON Parser Tests =============

void test_parse_combined_trade() {
    std::string raw = R"({"stream":"btcusdt@trade","data":{"e":"trade","E":123456789,"s":"BTCUSDT","t":1,"p":"43000.50000000","q":"0.10000000","b":1,"a":2,"T":123456789,"m":false,"M":true}})";
    auto parsed = parse_combined_message(raw);
    ASSERT_EQ(parsed.stream_kind, StreamKind::Trade);
    ASSERT_EQ(parsed.symbol_upper, std::string("BTCUSDT"));
    assert(parsed.data.contains("p"));
}

void test_parse_combined_depth() {
    std::string raw = R"({"stream":"btcusdt@depth@100ms","data":{"e":"depthUpdate","E":123,"s":"BTCUSDT","U":100,"u":105,"b":[["43000.50","1.0"]],"a":[["43001.00","0.5"]]}})";
    auto parsed = parse_combined_message(raw);
    ASSERT_EQ(parsed.stream_kind, StreamKind::DepthDiff);
    ASSERT_EQ(parsed.symbol_upper, std::string("BTCUSDT"));
}

void test_parse_depth_update_with_ba() {
    auto data = nlohmann::json::parse(R"({"e":"depthUpdate","s":"BTCUSDT","U":100,"u":105,"b":[["43000.50000000","1.20000000"]],"a":[["43001.00000000","0.50000000"]]})");
    auto update = parse_depth_update(data, "BTCUSDT");
    ASSERT_EQ(update.symbol, std::string("BTCUSDT"));
    ASSERT_EQ(update.first_update_id.value(), uint64_t(100));
    ASSERT_EQ(update.final_update_id.value(), uint64_t(105));
    ASSERT_EQ(update.bids.size(), size_t(1));
    ASSERT_EQ(update.bids[0].price, 4300050000000LL);
    ASSERT_EQ(update.bids[0].qty, 120000000LL);
}

void test_parse_depth_update_with_pu() {
    auto data = nlohmann::json::parse(R"({"e":"depthUpdate","s":"BTCUSDT","U":200,"u":205,"pu":199,"b":[],"a":[["43001.00000000","0.50000000"]]})");
    auto update = parse_depth_update(data, "BTCUSDT");
    ASSERT_EQ(update.previous_final_update_id.value(), uint64_t(199));
}

void test_parse_depth5() {
    auto data = nlohmann::json::parse(R"({"lastUpdateId":1234567,"bids":[["43000.50000000","1.20000000"],["42999.00000000","0.80000000"]],"asks":[["43001.00000000","0.90000000"],["43002.00000000","0.70000000"]]})");
    auto snap = parse_depth5_snapshot(data, "BTCUSDT");
    ASSERT_EQ(snap.last_update_id.value(), uint64_t(1234567));
    ASSERT_EQ(snap.bids.size(), size_t(2));
    ASSERT_EQ(snap.asks.size(), size_t(2));
    ASSERT_EQ(snap.bids[0].price, 4300050000000LL);
}

void test_parse_trade() {
    auto data = nlohmann::json::parse(R"({"e":"trade","s":"BTCUSDT","p":"43001.00000000","q":"0.50000000","t":123,"b":1,"a":2,"T":123,"m":false,"M":true})");
    auto trade = parse_trade(data);
    ASSERT_EQ(trade.symbol, std::string("BTCUSDT"));
    ASSERT_EQ(trade.price, 4300100000000LL);
    ASSERT_EQ(trade.qty, 50000000LL);
}

// ============= Trade Parse Error Test (Issue 5) =============

void test_trade_malformed_increments_parse_errors() {
    // Malformed trade missing "p" should cause parse error in process_event_logic
    std::string out_dir = "/tmp/test_trade_err";
    std::filesystem::create_directories(out_dir);

    EventProcessor processor(Venue::Spot, {"BTCUSDT"}, out_dir, "2025-01-01", true);

    MarketEvent event;
    event.recv_ts = {1700000000, 0};
    event.venue = Venue::Spot;
    event.stream_kind = StreamKind::Trade;
    event.shard_id = 0;
    event.conn_epoch = 0;
    event.conn_seq = 0;
    event.symbol = "BTCUSDT";
    event.payload_json = R"({"e":"trade","s":"BTCUSDT","q":"0.5"})"; // missing "p"

    processor.process_market_event_from_replay(event);
    ASSERT_EQ(processor.counters().parse_errors, uint64_t(1));
    ASSERT_EQ(processor.counters().trades_seen, uint64_t(0));

    // Valid trade increments trades_seen, no orderbook row
    event.payload_json = R"({"e":"trade","s":"BTCUSDT","p":"100.0","q":"0.5"})";
    processor.process_market_event_from_replay(event);
    ASSERT_EQ(processor.counters().trades_seen, uint64_t(1));
    ASSERT_EQ(processor.counters().orderbook_rows_written, uint64_t(0));

    std::filesystem::remove_all(out_dir);
}

// ============= Replay Symbol Order Test (Issue 4) =============

void test_replay_symbol_order() {
    // Create a fixture where ETHUSDT appears before BTCUSDT
    std::string tmpfile = "/tmp/test_replay_order.csv";
    {
        CsvWriter writer(tmpfile);
        writer.write_header({"recv_tsec", "recv_tnsec", "venue", "stream_kind",
                             "shard_id", "conn_epoch", "conn_seq", "symbol", "payload_json"});
        // ETHUSDT first
        std::string eth_depth5 = R"({"lastUpdateId":100,"bids":[["2000.00000000","1.00000000"]],"asks":[["2001.00000000","1.00000000"]]})";
        writer.write_row({"1700000001", "0", "spot", "depth5", "0", "0", "0", "ETHUSDT", eth_depth5});
        // BTCUSDT second
        std::string btc_depth5 = R"({"lastUpdateId":200,"bids":[["43000.00000000","1.00000000"]],"asks":[["43001.00000000","1.00000000"]]})";
        writer.write_row({"1700000002", "0", "spot", "depth5", "0", "0", "1", "BTCUSDT", btc_depth5});
    }

    std::string out_dir = "/tmp/test_replay_order_out";
    std::filesystem::create_directories(out_dir);

    int rc = run_replay(tmpfile, out_dir);
    ASSERT_EQ(rc, 0);

    // Check ETHUSDT orderbook has id=1, BTCUSDT has id=2
    for (auto& entry : std::filesystem::directory_iterator(out_dir)) {
        std::string name = entry.path().filename().string();
        if (name.find("ETHUSDT") != std::string::npos && name.find("orderbook") != std::string::npos) {
            CsvReader reader(entry.path().string());
            std::vector<std::string> row;
            if (!reader.read_row(row)) throw std::runtime_error("no header");
            if (!reader.read_row(row)) throw std::runtime_error("no data");
            // id column is index 3
            ASSERT_EQ(row[3], std::string("1"));
        }
        if (name.find("BTCUSDT") != std::string::npos && name.find("orderbook") != std::string::npos) {
            CsvReader reader(entry.path().string());
            std::vector<std::string> row;
            if (!reader.read_row(row)) throw std::runtime_error("no header");
            if (!reader.read_row(row)) throw std::runtime_error("no data");
            ASSERT_EQ(row[3], std::string("2"));
        }
    }

    std::filesystem::remove_all(out_dir);
    std::remove(tmpfile.c_str());
}

// ============= Replay Test =============

void test_replay_fixture() {
    std::string fixture = std::string(SOURCE_DIR) + "/tests/fixtures/sample_market_data.csv";
    std::string out_dir = "/tmp/test_replay_output";
    std::filesystem::create_directories(out_dir);

    int rc = run_replay(fixture, out_dir);
    ASSERT_EQ(rc, 0);

    // Find orderbook file
    std::string ob_file;
    for (auto& entry : std::filesystem::directory_iterator(out_dir)) {
        std::string name = entry.path().filename().string();
        if (name.find("orderbook") != std::string::npos) {
            ob_file = entry.path().string();
            break;
        }
    }
    if (ob_file.empty()) throw std::runtime_error("No orderbook file found in " + out_dir);

    // Validate header and column count
    CsvReader reader(ob_file);
    std::vector<std::string> row;
    if (!reader.read_row(row)) throw std::runtime_error("Cannot read orderbook header");
    ASSERT_EQ(row.size(), size_t(26));

    std::string expected_header = "tsec,tnsec,seqNo,id,type,side,bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,ask_size2,ask_size3,ask_size4";
    std::string got_header;
    for (size_t i = 0; i < row.size(); i++) {
        if (i > 0) got_header += ',';
        got_header += row[i];
    }
    ASSERT_EQ(got_header, expected_header);

    // Count data rows (should be 2: depth5 + depth_diff; trade does not emit)
    int data_rows = 0;
    while (reader.read_row(row)) {
        ASSERT_EQ(row.size(), size_t(26));
        data_rows++;
    }
    if (data_rows < 2) throw std::runtime_error(
        "Expected at least 2 orderbook rows, got " + std::to_string(data_rows));

    // Cleanup
    std::filesystem::remove_all(out_dir);
}

// ============= Main =============

int main() {
    std::cout << "Running unit tests...\n\n";
    std::cout << "[Decimal Scaler]\n";
    TEST(decimal_1);
    TEST(decimal_1_23);
    TEST(decimal_tiny);
    TEST(decimal_large);
    TEST(decimal_zero);
    TEST(decimal_invalid_empty);
    TEST(decimal_invalid_dot);
    TEST(decimal_invalid_abc);
    TEST(decimal_invalid_two_dots);

    std::cout << "\n[CSV Escaping]\n";
    TEST(csv_escape_plain);
    TEST(csv_escape_comma);
    TEST(csv_escape_quote);
    TEST(csv_escape_newline);
    TEST(csv_escape_json);

    std::cout << "\n[CSV Reader]\n";
    TEST(csv_roundtrip);
    TEST(csv_reader_payload_json);
    TEST(csv_reader_unterminated_quote);

    std::cout << "\n[Order Book]\n";
    TEST(orderbook_bid_insert);
    TEST(orderbook_remove_level);
    TEST(orderbook_sort_descending_bids);
    TEST(orderbook_sort_ascending_asks);
    TEST(orderbook_missing_levels_zero);
    TEST(depth5_replaces_book);

    std::cout << "\n[Order Book Continuity]\n";
    TEST(continuity_spot_stale_diff);
    TEST(continuity_spot_applied);
    TEST(continuity_spot_gap);
    TEST(continuity_futures_applied);
    TEST(continuity_futures_gap);

    std::cout << "\n[JSON Parsers]\n";
    TEST(parse_combined_trade);
    TEST(parse_combined_depth);
    TEST(parse_depth_update_with_ba);
    TEST(parse_depth_update_with_pu);
    TEST(parse_depth5);
    TEST(parse_trade);

    std::cout << "\n[Trade Parse]\n";
    TEST(trade_malformed_increments_parse_errors);

    std::cout << "\n[Replay]\n";
    TEST(replay_symbol_order);
    TEST(replay_fixture);

    std::cout << "\n========================================\n";
    std::cout << "Results: " << tests_passed << "/" << tests_run << " passed\n";

    return (tests_passed == tests_run) ? 0 : 1;
}
