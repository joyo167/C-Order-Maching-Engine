#include "orderbook_v2.h"
#include <iostream>

void orderbook_v2::submit_limit_order(uint64_t id, uint64_t trader_id, Side side, double price, uint64_t quantity) {
    int64_t tick = price_to_tick(price);
    if (tick < 0 || tick > max_tick_) {
        return; // out of the representable range - known limitation
    }

    order incoming{id, trader_id, side, price, quantity};

    if (side == Side::Buy) {
        matchbuy(incoming);
        if (incoming.quantity > 0) {
            bids_by_tick[tick].push_back(incoming);
            order_location[id] = {Side::Buy, tick};
            if (best_bid_tick == -1 || tick > best_bid_tick) {
                best_bid_tick = tick;
            }
        }
    } else {
        matchsell(incoming);
        if (incoming.quantity > 0) {
            asks_by_tick[tick].push_back(incoming);
            order_location[id] = {Side::Sell, tick};
            if (best_ask_tick == -1 || tick < best_ask_tick) {
                best_ask_tick = tick;
            }
        }
    }
}

void orderbook_v2::submit_market_order(uint64_t id, uint64_t trader_id, Side side, uint64_t quantity) {
    // Same trick as v1, just at the tick level instead of the raw
    // double level: give the incoming order a price that maps to the
    // extreme edge of the representable range, so it's willing to
    // match against ANY real resting order.
    if (side == Side::Buy) {
        order incoming{id, trader_id, side, (double)max_tick_ / TICK_SCALE, quantity};
        matchbuy(incoming);
    } else {
        order incoming{id, trader_id, side, 0.0, quantity};
        matchsell(incoming);
    }
    // deliberately never rests, regardless of leftover quantity
}

bool orderbook_v2::cancel_order(uint64_t id) {
    auto it = order_location.find(id);
    if (it == order_location.end()) {
        return false;
    }

    Location loc = it->second;
    auto& level = (loc.side == Side::Buy) ? bids_by_tick[loc.tick] : asks_by_tick[loc.tick];

    auto order_it = std::find_if(level.begin(), level.end(),
        [id](const order& o) { return o.id == id; });

    if (order_it == level.end()) {
        order_location.erase(it); // shouldn't happen if state stays consistent, but stay safe
        return false;
    }

    level.erase(order_it);
    order_location.erase(it);

    // Only need to fix up the persistent best-price pointer if we just
    // emptied the level that WAS the current best - emptying any
    // other level doesn't affect it at all.
    if (level.empty()) {
        if (loc.side == Side::Buy && loc.tick == best_bid_tick) {
            advance_best_bid();
        } else if (loc.side == Side::Sell && loc.tick == best_ask_tick) {
            advance_best_ask();
        }
    }
    return true;
}

void orderbook_v2::advance_best_ask() {
    while (best_ask_tick != -1 && best_ask_tick <= max_tick_ && asks_by_tick[best_ask_tick].empty()) {
        ++best_ask_tick;
    }
    if (best_ask_tick > max_tick_) {
        best_ask_tick = -1;
    }
}

void orderbook_v2::advance_best_bid() {
    while (best_bid_tick != -1 && best_bid_tick >= 0 && bids_by_tick[best_bid_tick].empty()) {
        --best_bid_tick;
    }
    if (best_bid_tick < 0) {
        best_bid_tick = -1;
    }
}

void orderbook_v2::matchbuy(order& incoming) {
    int64_t limit_tick = price_to_tick(incoming.price);
    int64_t tick = best_ask_tick; // LOCAL cursor for this call only

    while (incoming.quantity > 0 && tick != -1 && tick <= limit_tick && tick <= max_tick_) {
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

        ++tick; // move to the next tick regardless of why we stopped at this one
    }

    // Fix up the PERSISTENT best_ask_tick once, at the end, rather than
    // mid-loop - it just re-scans the same range we already touched.
    advance_best_ask();
}

void orderbook_v2::matchsell(order& incoming) {
    int64_t limit_tick = price_to_tick(incoming.price);
    int64_t tick = best_bid_tick;

    while (incoming.quantity > 0 && tick != -1 && tick >= limit_tick && tick >= 0) {
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

        --tick;
    }

    advance_best_bid();
}

void orderbook_v2::printbook() const {
    std::cout << "----------JOYO ORDERBOOK (v2 - tick array)----------" << "\n";
    std::cout << "ASKS (highest at top)" << "\n";
    for (int64_t t = std::min(max_tick_, (int64_t)(best_ask_tick == -1 ? -1 : max_tick_)); t >= 0; --t) {
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