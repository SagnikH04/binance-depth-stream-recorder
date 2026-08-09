#include "json_parsers.hpp"
#include "decimal_scaler.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

static std::string to_upper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

static StreamKind determine_stream_kind(const std::string& stream) {
    if (stream.find("@depth@") != std::string::npos ||
        stream.find("@depth@100ms") != std::string::npos) {
        return StreamKind::DepthDiff;
    }
    if (stream.find("@depth5") != std::string::npos) {
        return StreamKind::Depth5;
    }
    if (stream.find("@trade") != std::string::npos) {
        return StreamKind::Trade;
    }
    throw std::runtime_error("Unknown stream type: " + stream);
}

static std::string extract_symbol_from_stream(const std::string& stream) {
    auto pos = stream.find('@');
    if (pos == std::string::npos) {
        throw std::runtime_error("Cannot extract symbol from stream: " + stream);
    }
    return to_upper(stream.substr(0, pos));
}

ParsedCombinedMessage parse_combined_message(const std::string& raw) {
    auto j = nlohmann::json::parse(raw);

    if (!j.contains("stream") || !j.contains("data")) {
        throw std::runtime_error("Missing 'stream' or 'data' in combined message");
    }

    ParsedCombinedMessage msg;
    msg.stream = j["stream"].get<std::string>();
    msg.data = j["data"];
    msg.stream_kind = determine_stream_kind(msg.stream);
    msg.symbol_upper = extract_symbol_from_stream(msg.stream);
    msg.payload_json_minified = msg.data.dump();

    return msg;
}

static std::vector<LevelUpdate> parse_levels(const nlohmann::json& arr) {
    std::vector<LevelUpdate> levels;
    levels.reserve(arr.size());
    for (const auto& item : arr) {
        if (!item.is_array() || item.size() < 2) continue;
        LevelUpdate lvl;
        std::string price_str = item[0].get<std::string>();
        std::string qty_str = item[1].get<std::string>();
        lvl.price = parse_scaled_decimal(price_str, PRICE_SCALE);
        lvl.qty = parse_scaled_decimal(qty_str, QTY_SCALE);
        levels.push_back(lvl);
    }
    return levels;
}

DepthUpdate parse_depth_update(const nlohmann::json& data, const std::string& fallback_symbol) {
    DepthUpdate update;

    if (data.contains("s")) {
        update.symbol = to_upper(data["s"].get<std::string>());
    } else {
        update.symbol = fallback_symbol;
    }

    if (data.contains("U")) {
        update.first_update_id = data["U"].get<uint64_t>();
    }
    if (data.contains("u")) {
        update.final_update_id = data["u"].get<uint64_t>();
    }
    if (data.contains("pu")) {
        update.previous_final_update_id = data["pu"].get<uint64_t>();
    }

    if (data.contains("b") && data["b"].is_array()) {
        update.bids = parse_levels(data["b"]);
    }
    if (data.contains("a") && data["a"].is_array()) {
        update.asks = parse_levels(data["a"]);
    }

    return update;
}

Depth5Snapshot parse_depth5_snapshot(const nlohmann::json& data, const std::string& fallback_symbol) {
    Depth5Snapshot snapshot;

    if (data.contains("s")) {
        snapshot.symbol = to_upper(data["s"].get<std::string>());
    } else {
        snapshot.symbol = fallback_symbol;
    }

    if (data.contains("lastUpdateId")) {
        snapshot.last_update_id = data["lastUpdateId"].get<uint64_t>();
    }

    // bids: try "bids" first, then "b"
    if (data.contains("bids") && data["bids"].is_array()) {
        snapshot.bids = parse_levels(data["bids"]);
    } else if (data.contains("b") && data["b"].is_array()) {
        snapshot.bids = parse_levels(data["b"]);
    }

    // asks: try "asks" first, then "a"
    if (data.contains("asks") && data["asks"].is_array()) {
        snapshot.asks = parse_levels(data["asks"]);
    } else if (data.contains("a") && data["a"].is_array()) {
        snapshot.asks = parse_levels(data["a"]);
    }

    return snapshot;
}

TradeEvent parse_trade(const nlohmann::json& data) {
    TradeEvent trade;
    trade.symbol = to_upper(data["s"].get<std::string>());
    trade.price = parse_scaled_decimal(data["p"].get<std::string>(), PRICE_SCALE);
    trade.qty = parse_scaled_decimal(data["q"].get<std::string>(), QTY_SCALE);
    return trade;
}
