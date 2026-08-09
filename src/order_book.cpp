#include "order_book.hpp"

void OrderBook::apply_depth5(const Depth5Snapshot& snapshot) {
    bids_.clear();
    asks_.clear();

    for (const auto& lvl : snapshot.bids) {
        if (lvl.qty != 0) {
            bids_[lvl.price] = lvl.qty;
        }
    }
    for (const auto& lvl : snapshot.asks) {
        if (lvl.qty != 0) {
            asks_[lvl.price] = lvl.qty;
        }
    }

    if (snapshot.last_update_id.has_value()) {
        last_update_id_ = snapshot.last_update_id;
    }
    initialized_ = true;
    stale_ = false;
}

ApplyResult OrderBook::apply_depth_update(const DepthUpdate& update) {
    if (!initialized_ || stale_) {
        return ApplyResult::SkippedStaleOrOld;
    }

    if (last_update_id_.has_value()) {
        // 1. Stale check: if final_update_id <= last_update_id, skip
        if (update.final_update_id.has_value() &&
            *update.final_update_id <= *last_update_id_) {
            return ApplyResult::SkippedStaleOrOld;
        }

        // 2. Futures-style: pu must equal last_update_id
        if (update.previous_final_update_id.has_value()) {
            if (*update.previous_final_update_id != *last_update_id_) {
                return ApplyResult::GapDetected;
            }
        }
        // 3. Spot-style bridge rule: U <= last_update_id+1 <= u
        else if (update.first_update_id.has_value() && update.final_update_id.has_value()) {
            uint64_t expected = *last_update_id_ + 1;
            if (!(*update.first_update_id <= expected && expected <= *update.final_update_id)) {
                return ApplyResult::GapDetected;
            }
        }
    }

    // Apply bid updates
    for (const auto& lvl : update.bids) {
        if (lvl.qty == 0) {
            bids_.erase(lvl.price);
        } else {
            bids_[lvl.price] = lvl.qty;
        }
    }

    // Apply ask updates
    for (const auto& lvl : update.asks) {
        if (lvl.qty == 0) {
            asks_.erase(lvl.price);
        } else {
            asks_[lvl.price] = lvl.qty;
        }
    }

    if (update.final_update_id.has_value()) {
        last_update_id_ = update.final_update_id;
    }

    return ApplyResult::Applied;
}

void OrderBook::clear_and_mark_stale() {
    bids_.clear();
    asks_.clear();
    initialized_ = false;
    stale_ = true;
    last_update_id_.reset();
}

TopOfBookSnapshot OrderBook::top5() const {
    TopOfBookSnapshot snap{};
    snap.bid_prices.fill(0);
    snap.bid_sizes.fill(0);
    snap.ask_prices.fill(0);
    snap.ask_sizes.fill(0);

    int i = 0;
    for (auto it = bids_.begin(); it != bids_.end() && i < 5; ++it, ++i) {
        snap.bid_prices[i] = it->first;
        snap.bid_sizes[i] = it->second;
    }

    i = 0;
    for (auto it = asks_.begin(); it != asks_.end() && i < 5; ++it, ++i) {
        snap.ask_prices[i] = it->first;
        snap.ask_sizes[i] = it->second;
    }

    return snap;
}
