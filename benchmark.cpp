#include "orderbook.h"
#include "orderbook_v2.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>

using std::cout;

struct MockOrder {
    bool is_market;
    uint64_t trader_id;
    Side side;
    double price;
    uint64_t quantity;
};

uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

void print_stats(const std::string& name, uint64_t total_ns, std::vector<uint64_t>& latencies, size_t num_orders, size_t num_trades) {
    std::sort(latencies.begin(), latencies.end());
    uint64_t median = latencies[num_orders / 2];
    uint64_t p90 = latencies[num_orders * 0.90];
    uint64_t p99 = latencies[num_orders * 0.99];
    uint64_t max_lat = latencies.back();
    
    double total_seconds = total_ns / 1'000'000'000.0;
    double throughput = num_orders / total_seconds;

    cout << "\n=== BENCHMARK RESULTS (" << name << ") ===\n";
    cout << "Total Orders: " << num_orders << "\n";
    cout << "Total Trades Executed: " << num_trades << "\n";
    cout << "Total Time: " << total_seconds << " seconds\n";
    cout << "Throughput: " << (uint64_t)throughput << " orders/second\n\n";
    cout << "--- Latency per order ---\n";
    cout << "Median: " << median << " ns\n";
    cout << "p90:    " << p90 << " ns\n";
    cout << "p99:    " << p99 << " ns\n";
    cout << "Max:    " << max_lat << " ns\n";
    cout << "=======================================\n";
}

int main() {
    const int NUM_ORDERS = 1000000;
    std::vector<MockOrder> test_orders;
    test_orders.reserve(NUM_ORDERS);

    cout << "Generating " << NUM_ORDERS << " random orders...\n";

    std::mt19937 rng(42); 
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> type_dist(1, 100); 
    std::uniform_int_distribution<uint64_t> qty_dist(1, 100);
    std::uniform_int_distribution<uint64_t> trader_dist(1, 1000); 
    std::uniform_real_distribution<double> price_dist(95.00, 105.0); // Ultra-tight spread to avoid sparse array traversal

    for (int i = 0; i < NUM_ORDERS; ++i) {
        MockOrder o;
        o.is_market = (type_dist(rng) <= 10);
        o.side = (side_dist(rng) == 0) ? Side::Buy : Side::Sell;
        o.price = std::round(price_dist(rng) * 100.0) / 100.0; 
        o.quantity = qty_dist(rng);
        o.trader_id = trader_dist(rng);
        test_orders.push_back(o);
    }

    // ---------------------------------------------------------
    // Run v1 (std::map)
    // ---------------------------------------------------------
    {
        orderbook book_v1;
        std::vector<uint64_t> latencies_v1;
        latencies_v1.reserve(NUM_ORDERS);

        cout << "Running v1 (std::map)..." << std::endl;
        uint64_t start_v1 = now_ns();

        for (uint64_t i = 0; i < NUM_ORDERS; ++i) {
            const MockOrder& o = test_orders[i];
            uint64_t s = now_ns();
            
            if (o.is_market) book_v1.submit_market_order(i + 1, o.trader_id, o.side, o.quantity);
            else book_v1.submit_limit_order(i + 1, o.trader_id, o.side, o.price, o.quantity);

            latencies_v1.push_back(now_ns() - s);
        }
        
        uint64_t end_v1 = now_ns();
        print_stats("v1 std::map", end_v1 - start_v1, latencies_v1, NUM_ORDERS, book_v1.get_trades().size());
    }

    // ---------------------------------------------------------
    // Run v2 (Flat Array)
    // ---------------------------------------------------------
    {
        orderbook_v2 book_v2; // Max price defaults to 1000.0
        std::vector<uint64_t> latencies_v2;
        latencies_v2.reserve(NUM_ORDERS);

        cout << "Running v2 (Flat Array)..." << std::endl;
        uint64_t start_v2 = now_ns();

        for (uint64_t i = 0; i < NUM_ORDERS; ++i) {
            const MockOrder& o = test_orders[i];
            uint64_t s = now_ns();
            
            if (o.is_market) book_v2.submit_market_order(i + 1, o.trader_id, o.side, o.quantity);
            else book_v2.submit_limit_order(i + 1, o.trader_id, o.side, o.price, o.quantity);

            latencies_v2.push_back(now_ns() - s);
        }
        
        uint64_t end_v2 = now_ns();
        print_stats("v2 Flat Array", end_v2 - start_v2, latencies_v2, NUM_ORDERS, book_v2.get_trades().size());
    }

    return 0;
}