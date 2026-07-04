#pragma once
#include "order.h"
#include "trade.h"
#include <vector>
#include <deque>
#include <unordered_map>
#include <cmath>
#include <algorithm>

// V3 = V2's tick array, PLUS a bitmap that tracks which ticks are
// occupied. Finding "the next occupied tick" no longer means walking
// past empty array slots one at a time - it means finding the next
// set bit, which the CPU can do across 64 ticks in a single
// instruction. This is what fixes v2's wide-spread weakness without
// needing prices to cluster tightly.
class orderbook_v3 {
public:
    explicit orderbook_v3(int64_t max_tick = 1000000)
        : max_tick_(max_tick),
          asks_by_tick(max_tick + 1),
          bids_by_tick(max_tick + 1),
          ask_bitmap(max_tick / 64 + 1, 0),
          bid_bitmap(max_tick / 64 + 1, 0) {
        order_location.reserve(max_tick + 1); // the rehashing lesson, applied
    }

    void submit_limit_order(uint64_t id, uint64_t trader_id, Side side, double price, uint64_t quantity);
    void submit_market_order(uint64_t id, uint64_t trader_id, Side side, uint64_t quantity);
    bool cancel_order(uint64_t id);
    void printbook() const;
    const std::vector<trade>& get_trades() const { return trades; }

private:
    static constexpr int64_t TICK_SCALE = 100;

    int64_t max_tick_;
    std::vector<std::deque<order>> asks_by_tick;
    std::vector<std::deque<order>> bids_by_tick;

    // One bit per tick: 1 = "this tick has at least one resting order".
    std::vector<uint64_t> ask_bitmap;
    std::vector<uint64_t> bid_bitmap;

    int64_t best_ask_tick = -1;
    int64_t best_bid_tick = -1;

    struct Location { Side side; int64_t tick; };
    std::unordered_map<uint64_t, Location> order_location;

    std::vector<trade> trades;

    static int64_t price_to_tick(double price) {
        return (int64_t)std::round(price * TICK_SCALE);
    }

    void advance_best_ask();
    void advance_best_bid();
    void matchbuy(order& incoming);
    void matchsell(order& incoming);
};