#pragma once
#include "market_event.hpp"
#include <nlohmann/json.hpp>
#include <string>

struct ParsedCombinedMessage {
    std::string stream;
    std::string symbol_upper;
    StreamKind stream_kind;
    nlohmann::json data;
    std::string payload_json_minified;
};

ParsedCombinedMessage parse_combined_message(const std::string& raw);
DepthUpdate parse_depth_update(const nlohmann::json& data, const std::string& fallback_symbol);
Depth5Snapshot parse_depth5_snapshot(const nlohmann::json& data, const std::string& fallback_symbol);
TradeEvent parse_trade(const nlohmann::json& data);
