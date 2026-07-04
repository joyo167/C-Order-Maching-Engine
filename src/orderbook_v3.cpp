#include "orderbook_v3.h"
#include <iostream>

namespace {

// Sets/clears one bit for a given tick.
inline void set_bit(std::vector<uint64_t>& bitmap, int64_t tick) {
    bitmap[tick / 64] |= (1ULL << (tick % 64));
}
inline void clear_bit(std::vector<uint64_t>& bitmap, int64_t tick) {
    bitmap[tick / 64] &= ~(1ULL << (tick % 64));
}

// Finds the next set bit at or after `from`, scanning UPWARD.
// __builtin_ctzll ("count trailing zeros") finds the lowest set bit
// within a 64-bit word in one CPU instruction - this is what lets us
// skip 64 ticks at a time instead of checking them one by one.
// Returns -1 if nothing is set from `from` onward.
int64_t find_next_set(const std::vector<uint64_t>& bitmap, int64_t from) {
    if (from < 0) from = 0;
    int64_t word = from / 64;
    int bit = from % 64;
    if (word >= (int64_t)bitmap.size()) return -1;

    uint64_t w = bitmap[word] >> bit;
    if (w) return word * 64 + bit + __builtin_ctzll(w);

    for (++word; word < (int64_t)bitmap.size(); ++word) {
        if (bitmap[word]) return word * 64 + __builtin_ctzll(bitmap[word]);
    }
    return -1;
}

// Finds the next set bit at or before `from`, scanning DOWNWARD.
// __builtin_clzll ("count leading zeros") finds the highest set bit.
int64_t find_prev_set(const std::vector<uint64_t>& bitmap, int64_t from) {
    if (from < 0) return -1;
    int64_t word = from / 64;
    int bit = from % 64;
    if (word >= (int64_t)bitmap.size()) word = (int64_t)bitmap.size() - 1;

    uint64_t mask = (bit == 63) ? ~0ULL : ((1ULL << (bit + 1)) - 1);
    uint64_t w = bitmap[word] & mask;
    if (w) return word * 64 + (63 - __builtin_clzll(w));

    for (--word; word >= 0; --word) {
        if (bitmap[word]) return word * 64 + (63 - __builtin_clzll(bitmap[word]));
    }
    return -1;
}

} // anonymous namespace

void orderbook_v3::submit_limit_order(uint64_t id, uint64_t trader_id, Side side, double price, uint64_t quantity) {
    int64_t tick = price_to_tick(price);
    if (tick < 0 || tick > max_tick_) {
        return;
    }

    order incoming{id, trader_id, side, price, quantity};

    if (side == Side::Buy) {
        matchbuy(incoming);
        if (incoming.quantity > 0) {
            bids_by_tick[tick].push_back(incoming);
            set_bit(bid_bitmap, tick);
            order_location[id] = {Side::Buy, tick};
            if (best_bid_tick == -1 || tick > best_bid_tick) {
                best_bid_tick = tick;
            }
        }
    } else {
        matchsell(incoming);
        if (incoming.quantity > 0) {
            asks_by_tick[tick].push_back(incoming);
            set_bit(ask_bitmap, tick);
            order_location[id] = {Side::Sell, tick};
            if (best_ask_tick == -1 || tick < best_ask_tick) {
                best_ask_tick = tick;
            }
        }
    }
}

void orderbook_v3::submit_market_order(uint64_t id, uint64_t trader_id, Side side, uint64_t quantity) {
    if (side == Side::Buy) {
        order incoming{id, trader_id, side, (double)max_tick_ / TICK_SCALE, quantity};
        matchbuy(incoming);
    } else {
        order incoming{id, trader_id, side, 0.0, quantity};
        matchsell(incoming);
    }
}

bool orderbook_v3::cancel_order(uint64_t id) {
    auto it = order_location.find(id);
    if (it == order_location.end()) {
        return false;
    }

    Location loc = it->second;
    auto& level = (loc.side == Side::Buy) ? bids_by_tick[loc.tick] : asks_by_tick[loc.tick];

    auto order_it = std::find_if(level.begin(), level.end(),
        [id](const order& o) { return o.id == id; });

    if (order_it == level.end()) {
        order_location.erase(it);
        return false;
    }

    level.erase(order_it);
    order_location.erase(it);

    if (level.empty()) {
        if (loc.side == Side::Buy) {
            clear_bit(bid_bitmap, loc.tick);
            if (loc.tick == best_bid_tick) advance_best_bid();
        } else {
            clear_bit(ask_bitmap, loc.tick);
            if (loc.tick == best_ask_tick) advance_best_ask();
        }
    }
    return true;
}

void orderbook_v3::advance_best_ask() {
    if (best_ask_tick == -1) return;
    best_ask_tick = find_next_set(ask_bitmap, best_ask_tick);
}

void orderbook_v3::advance_best_bid() {
    if (best_bid_tick == -1) return;
    best_bid_tick = find_prev_set(bid_bitmap, best_bid_tick);
}

void orderbook_v3::matchbuy(order& incoming) {
    int64_t limit_tick = price_to_tick(incoming.price);
    int64_t tick = best_ask_tick;

    while (incoming.quantity > 0 && tick != -1 && tick <= limit_tick) {
        auto& level = asks_by_tick[tick];
        auto resting = level.begin();

        while (resting != level.end() && incoming.quantity > 0) {
            if (resting->trader_id == incoming.trader_id) {
                ++resting;
                continue;
            }

            uint64_t traded = std::min(incoming.quantity, resting->quantity);
            trades.push_back(trade{incoming.id, resting->id, (double)tick / TICK_SCALE, traded});

            incoming.quantity -= traded;
            resting->quantity -= traded;

            if (resting->quantity == 0) {
                order_location.erase(resting->id);
                resting = level.erase(resting);
            } else {
                ++resting;
            }
        }

        if (level.empty()) {
            clear_bit(ask_bitmap, tick);
        }

        // Jump straight to the next OCCUPIED tick - not just tick+1.
        // This is the line that actually fixes the wide-spread problem.
        tick = find_next_set(ask_bitmap, tick + 1);
    }

    advance_best_ask();
}

void orderbook_v3::matchsell(order& incoming) {
    int64_t limit_tick = price_to_tick(incoming.price);
    int64_t tick = best_bid_tick;

    while (incoming.quantity > 0 && tick != -1 && tick >= limit_tick) {
        auto& level = bids_by_tick[tick];
        auto resting = level.begin();

        while (resting != level.end() && incoming.quantity > 0) {
            if (resting->trader_id == incoming.trader_id) {
                ++resting;
                continue;
            }

            uint64_t traded = std::min(incoming.quantity, resting->quantity);
            trades.push_back(trade{resting->id, incoming.id, (double)tick / TICK_SCALE, traded});

            incoming.quantity -= traded;
            resting->quantity -= traded;

            if (resting->quantity == 0) {
                order_location.erase(resting->id);
                resting = level.erase(resting);
            } else {
                ++resting;
            }
        }

        if (level.empty()) {
            clear_bit(bid_bitmap, tick);
        }

        tick = (tick == 0) ? -1 : find_prev_set(bid_bitmap, tick - 1);
    }

    advance_best_bid();
}

void orderbook_v3::printbook() const {
    std::cout << "----------JOYO ORDERBOOK (v3 - bitmap+array)----------" << "\n";
    std::cout << "ASKS (highest at top)" << "\n";
    for (int64_t t = max_tick_; t >= 0; --t) {
        if (!asks_by_tick[t].empty()) {
            for (const auto& o : asks_by_tick[t]) {
                std::cout << "[" << (double)t / TICK_SCALE << "] id=" << o.id
                          << " trader=" << o.trader_id << " qty= " << o.quantity << "\n";
            }
        }
    }
    std::cout << "-------------------" << "\n";
    std::cout << "BIDS (highest at top)" << "\n";
    for (int64_t t = max_tick_; t >= 0; --t) {
        if (!bids_by_tick[t].empty()) {
            for (const auto& o : bids_by_tick[t]) {
                std::cout << "[" << (double)t / TICK_SCALE << "] id=" << o.id
                          << " trader=" << o.trader_id << " qty= " << o.quantity << "\n";
            }
        }
    }
    std::cout << "-------------------" << "\n";
}