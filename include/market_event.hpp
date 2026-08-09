#pragma once
#include "time_utils.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class Venue { Spot, Usdm };

enum class StreamKind { DepthDiff, Depth5, Trade };

struct MarketEvent {
    ReceiveTimestamp recv_ts;
    Venue venue;
    StreamKind stream_kind;
    uint32_t shard_id;
    uint32_t conn_epoch;
    uint64_t conn_seq;
    std::string symbol;
    std::string payload_json;
};

struct LevelUpdate {
    int64_t price;
    int64_t qty;
};

struct DepthUpdate {
    std::string symbol;
    std::optional<uint64_t> first_update_id;  // U
    std::optional<uint64_t> final_update_id;  // u
    std::optional<uint64_t> previous_final_update_id;  // pu
    std::vector<LevelUpdate> bids;
    std::vector<LevelUpdate> asks;
};

struct Depth5Snapshot {
    std::string symbol;
    std::optional<uint64_t> last_update_id;
    std::vector<LevelUpdate> bids;
    std::vector<LevelUpdate> asks;
};

struct TradeEvent {
    std::string symbol;
    int64_t price;
    int64_t qty;
};

struct TopOfBookSnapshot {
    std::array<int64_t, 5> bid_prices;
    std::array<int64_t, 5> bid_sizes;
    std::array<int64_t, 5> ask_prices;
    std::array<int64_t, 5> ask_sizes;
};

inline std::string venue_to_string(Venue v) {
    switch (v) {
        case Venue::Spot: return "spot";
        case Venue::Usdm: return "usdm";
    }
    return "unknown";
}

inline Venue parse_venue(const std::string& s) {
    if (s == "spot") return Venue::Spot;
    if (s == "usdm") return Venue::Usdm;
    throw std::invalid_argument("Unknown venue: " + s);
}

inline std::string stream_kind_to_string(StreamKind sk) {
    switch (sk) {
        case StreamKind::DepthDiff: return "depth_diff";
        case StreamKind::Depth5: return "depth5";
        case StreamKind::Trade: return "trade";
    }
    return "unknown";
}

inline StreamKind parse_stream_kind(const std::string& s) {
    if (s == "depth_diff") return StreamKind::DepthDiff;
    if (s == "depth5") return StreamKind::Depth5;
    if (s == "trade") return StreamKind::Trade;
    throw std::invalid_argument("Unknown stream_kind: " + s);
}
