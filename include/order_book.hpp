#pragma once
#include "market_event.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <functional>

enum class ApplyResult {
    Applied,
    SkippedStaleOrOld,
    GapDetected
};

class OrderBook {
public:
    OrderBook() = default;

    void apply_depth5(const Depth5Snapshot& snapshot);
    ApplyResult apply_depth_update(const DepthUpdate& update);
    void clear_and_mark_stale();
    TopOfBookSnapshot top5() const;

    bool is_initialized() const { return initialized_; }
    bool is_stale() const { return stale_; }

private:
    std::map<int64_t, int64_t, std::greater<int64_t>> bids_;
    std::map<int64_t, int64_t> asks_;
    bool initialized_ = false;
    bool stale_ = true;
    std::optional<uint64_t> last_update_id_;
};
