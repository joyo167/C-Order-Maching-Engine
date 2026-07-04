#pragma once
#include "order.h"
#include "trade.h"
#include <vector>
#include <deque>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <algorithm>

// Determines the precision of our flat array.
// 100.0 means 2 decimal places (cents). $101.50 -> tick 10150.
constexpr double TICK_SCALE = 100.0;

// The Location struct used for O(1) cancellations
struct Location {
    Side side;
    int64_t tick;
};

class orderbook_v2 {
public:
    // Constructor: we pre-allocate the array based on the max possible price.
    // E.g., max_price = 1000.0 means max_tick = 100,000
    explicit orderbook_v2(double max_price = 1000.0) 
        : max_tick_(std::round(max_price * TICK_SCALE)),
          bids_by_tick(max_tick_ + 1),
          asks_by_tick(max_tick_ + 1),
          best_bid_tick(-1),
          best_ask_tick(-1) {
        order_location.reserve(max_tick_ + 1); // avoids rehashing as it fills up
    }

    void submit_limit_order(uint64_t id, uint64_t trader_id, Side side, double price, uint64_t quantity);
    void submit_market_order(uint64_t id, uint64_t trader_id, Side side, uint64_t quantity);
    bool cancel_order(uint64_t id);
    void printbook() const;
    const std::vector<trade>& get_trades() const { return trades; }

private:
    int64_t max_tick_;

    // The Flat Arrays replacing std::map
    // Index is the tick. Value is the queue of orders at that price.
    std::vector<std::deque<order>> bids_by_tick;
    std::vector<std::deque<order>> asks_by_tick;

    // O(1) lookup map for fast cancellations
    std::unordered_map<uint64_t, Location> order_location;

    // Track the absolute best prices to avoid scanning the whole array
    int64_t best_bid_tick;
    int64_t best_ask_tick;

    std::vector<trade> trades;

    // Helper to convert double price to integer array index
    int64_t price_to_tick(double price) const {
        return std::round(price * TICK_SCALE);
    }

    void matchbuy(order& incoming);
    void matchsell(order& incoming);
    
    // Helpers to quickly find the next best price when a level is emptied
    void advance_best_ask();
    void advance_best_bid();
};