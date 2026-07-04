#include "orderbook.h"
#include "orderbook_v2.h"
#include "orderbook_v3.h"
#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>

using Clock = std::chrono::high_resolution_clock;

struct GeneratedOrder {
    uint64_t id;
    uint64_t trader_id;
    Side side;
    double price;
    uint64_t quantity;
    bool is_market;
};

std::vector<GeneratedOrder> generate_orders(int n, double low_price, double high_price, unsigned seed) {
    std::vector<GeneratedOrder> orders;
    orders.reserve(n);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> price_dist(low_price, high_price);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<uint64_t> qty_dist(1, 20);
    std::uniform_int_distribution<uint64_t> trader_dist(1, 1000);
    std::uniform_int_distribution<int> market_dist(0, 9); // ~10% market orders

    for (int i = 0; i < n; ++i) {
        GeneratedOrder o;
        o.id = i;
        o.trader_id = trader_dist(rng);
        o.side = side_dist(rng) == 0 ? Side::Buy : Side::Sell;
        o.price = std::round(price_dist(rng) * 100) / 100.0;
        o.quantity = qty_dist(rng);
        o.is_market = market_dist(rng) == 0;
        orders.push_back(o);
    }
    return orders;
}

template<typename Book>
void run_benchmark(const std::string& label, Book& book, const std::vector<GeneratedOrder>& orders) {
    std::vector<uint64_t> latencies;
    latencies.reserve(orders.size());

    auto total_start = Clock::now();
    for (const auto& o : orders) {
        auto start = Clock::now();
        if (o.is_market) {
            book.submit_market_order(o.id, o.trader_id, o.side, o.quantity);
        } else {
            book.submit_limit_order(o.id, o.trader_id, o.side, o.price, o.quantity);
        }
        auto end = Clock::now();
        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    auto total_end = Clock::now();

    double total_s = std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count() / 1e9;
    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();

    std::cout << "=== " << label << " ===" << "\n";
    std::cout << "Total Orders: " << n << "\n";
    std::cout << "Total Trades Executed: " << book.get_trades().size() << "\n";
    std::cout << "Total Time: " << total_s << " seconds\n";
    std::cout << "Throughput: " << (uint64_t)(n / total_s) << " orders/second\n";
    std::cout << "Median: " << latencies[n * 50 / 100] << " ns\n";
    std::cout << "p90:    " << latencies[n * 90 / 100] << " ns\n";
    std::cout << "p99:    " << latencies[n * 99 / 100] << " ns\n";
    std::cout << "Max:    " << latencies.back() << " ns\n";
    std::cout << "=======================================\n\n";
}

void run_all_three(const std::string& spread_label, double low, double high) {
    std::cout << "\n########## SPREAD: " << spread_label << " (" << low << " - " << high << ") ##########\n\n";

    auto orders1 = generate_orders(1000000, low, high, 42);
    auto orders2 = generate_orders(1000000, low, high, 42);
    auto orders3 = generate_orders(1000000, low, high, 42);

    {
        orderbook v1;
        run_benchmark("v1 (std::map)", v1, orders1);
    }
    {
        orderbook_v2 v2;
        run_benchmark("v2 (flat array, no bitmap)", v2, orders2);
    }
    {
        orderbook_v3 v3;
        run_benchmark("v3 (flat array + bitmap)", v3, orders3);
    }
}

int main() {
    run_all_three("NARROW", 99.90, 100.10);
    run_all_three("WIDE", 95.0, 105.0);
    return 0;
}