╔══════════════════════════════════════════════════════════════════════════════════════════╗
║                           LAYER 1: SHARED DATA STRUCTURES                               ║
║                    (Everything else in the project depends on these)                     ║
╠══════════════════════════════╦═══════════════════════════════════════════════════════════╣
║  include/order.h             ║  include/trade.h                                         ║
║  ─────────────────────────── ║  ─────────────────────────────────────────────────────── ║
║  #define ln "\n"             ║  struct trade {                                          ║
║                              ║      uint64_t buyorder_id;                               ║
║  enum class Side             ║      uint64_t sellorder_id;                              ║
║       { Buy, Sell }          ║      double   price;                                     ║
║                              ║      uint64_t quantity;                                  ║
║  struct order {              ║  }                                                        ║
║      uint64_t id;            ║                                                          ║
║      uint64_t trader_id;     ║  Recorded every time a buy and sell                      ║
║      Side     side;          ║  order match against each other.                         ║
║      double   price;         ║                                                          ║
║      uint64_t quantity;      ║                                                          ║
║  }                           ║                                                          ║
╚══════════════════════════════╩═══════════════════════════════════════════════════════════╝
                    │                              │
                    └──────────────┬───────────────┘
                                   │ Both included by all 3 engines below
                                   ▼
╔══════════════════════════════════════════════════════════════════════════════════════════╗
║                              LAYER 2: THE THREE ENGINES                                  ║
║                    (Same public interface. Different internal data structures.)          ║
╠══════════════════════╦═══════════════════════════════╦═══════════════════════════════════╣
║  V1 (std::map)       ║  V2 (Flat Array)              ║  V3 (Flat Array + Bitmap)         ║
║  ─────────────────── ║  ─────────────────────────── ║  ─────────────────────────────── ║
║  include/orderbook.h ║  include/orderbook_v2.h       ║  include/orderbook_v3.h          ║
║  src/orderbook.cpp   ║  src/orderbook_v2.cpp         ║  src/orderbook_v3.cpp            ║
╠══════════════════════╬═══════════════════════════════╬═══════════════════════════════════╣
║  PUBLIC API (all 3 engines expose exactly the same 5 functions):                        ║
║  submit_limit_order(id, trader_id, side, price, qty)                                    ║
║  submit_market_order(id, trader_id, side, qty)                                          ║
║  cancel_order(id) → bool                                                                ║
║  get_trades() → vector<trade>&                                                          ║
║  printbook()                                                                            ║
╠══════════════════════╬═══════════════════════════════╬═══════════════════════════════════╣
║  PRIVATE DATA:       ║  PRIVATE DATA:                ║  PRIVATE DATA:                    ║
║                      ║                               ║                                   ║
║  map<double,         ║  vector<deque<order>>         ║  vector<deque<order>>             ║
║    deque<order>,     ║    bids_by_tick[100001]       ║    bids_by_tick[1000001]          ║
║    greater<double>>  ║    asks_by_tick[100001]       ║    asks_by_tick[1000001]          ║
║    bids;             ║                               ║                                   ║
║                      ║  unordered_map                ║  unordered_map                    ║
║  map<double,         ║    <uint64_t, Location>       ║    <uint64_t, Location>           ║
║    deque<order>>     ║    order_location             ║    order_location                 ║
║    asks;             ║                               ║                                   ║
║                      ║  int64_t best_bid_tick        ║  int64_t best_bid_tick            ║
║  vector<trade>       ║  int64_t best_ask_tick        ║  int64_t best_ask_tick            ║
║    trades;           ║                               ║                                   ║
║                      ║  vector<trade> trades         ║  vector<uint64_t> bid_bitmap  ◄── NEW!
║                      ║                               ║  vector<uint64_t> ask_bitmap  ◄── NEW!
║                      ║                               ║                                   ║
║                      ║                               ║  vector<trade> trades             ║
╠══════════════════════╬═══════════════════════════════╬═══════════════════════════════════╣
║  CANCEL:             ║  CANCEL:                      ║  CANCEL:                          ║
║  Scans ALL levels    ║  order_location[id]           ║  order_location[id]               ║
║  O(N) — slowest      ║  → jump to exact tick. O(1)  ║  → jump to exact tick. O(1)       ║
╠══════════════════════╬═══════════════════════════════╬═══════════════════════════════════╣
║  NEXT PRICE:         ║  NEXT PRICE:                  ║  NEXT PRICE:                      ║
║  map iterator++      ║  ++tick (linear scan)         ║  find_next_set(bitmap, tick+1)    ║
║  instant. Always     ║  SLOW on wide spreads:        ║  uses __builtin_ctzll             ║
║  jumps to next       ║  may check 1000 empty         ║  skips 64 empty ticks in a        ║
║  active price. O(1)  ║  slots one by one.            ║  single CPU instruction. FAST     ║
║                      ║                               ║  on ALL spreads.                  ║
╠══════════════════════╬═══════════════════════════════╬═══════════════════════════════════╣
║  BENCHMARK RESULTS (1M orders, -O3, tight spread 99.90–100.10):                         ║
║  Throughput:  10.9M  ║  Throughput: 10.4M            ║  Throughput: 11.4M  ← WINNER     ║
║  Median:      42 ns  ║  Median:      42 ns           ║  Median:     42 ns               ║
║  p99:        250 ns  ║  p99:        250 ns           ║  p99:       250 ns               ║
╠══════════════════════╬═══════════════════════════════╬═══════════════════════════════════╣
║  BENCHMARK RESULTS (1M orders, -O3, wide spread 95.0–105.0):                            ║
║  Throughput:  10.6M  ║  Throughput:  6.1M            ║  Throughput: 13.2M  ← WINNER     ║
║  Median:      83 ns  ║  Median:      42 ns           ║  Median:     42 ns               ║
║  p99:        209 ns  ║  p99:       1000 ns           ║  p99:       208 ns               ║
╚══════════════════════╩═══════════════════════════════╩═══════════════════════════════════╝
           │                         │                              │
           └─────────────────────────┼──────────────────────────────┘
                                     │
                                     ▼
╔══════════════════════════════════════════════════════════════════════════════════════════╗
║                     LAYER 3: DATA FLOW INSIDE THE ENGINE                                ║
║         (What happens when you call submit_limit_order — using v3 as example)           ║
╠══════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                          ║
║  submit_limit_order(id=5, trader=200, Buy, price=101.50, qty=10)                        ║
║      │                                                                                   ║
║      ▼                                                                                   ║
║  tick = price_to_tick(101.50) = round(101.50 * 100) = 10150                             ║
║      │                                                                                   ║
║      ▼                                                                                   ║
║  side == Buy → call matchbuy(incoming)                                                   ║
║      │                                                                                   ║
║      │  tick = best_ask_tick  ← start at the current lowest ask                         ║
║      │  LOOP while (my_qty > 0  AND  tick <= 10150):                                    ║
║      │      level = asks_by_tick[tick]        ← O(1) array lookup                       ║
║      │      for each resting sell order:                                                 ║
║      │          if same trader_id  → skip     ← self-trade prevention                  ║
║      │          traded = min(my_qty, their_qty)                                         ║
║      │          trades.push_back(trade{...})  ← record the trade                        ║
║      │          reduce both quantities                                                   ║
║      │          if resting order fully filled:                                           ║
║      │              erase from deque                                                     ║
║      │              order_location.erase(id) ← remove from cancel map                  ║
║      │      if level is now empty:                                                       ║
║      │          clear_bit(ask_bitmap, tick)   ← v3: unmark this tick                   ║
║      │      tick = find_next_set(bitmap, tick+1)  ← v3: skip 64 ticks at a time!       ║
║      │                                                                                   ║
║      ▼                                                                                   ║
║  advance_best_ask()  ← update best price pointer after matching                         ║
║      │                                                                                   ║
║      ▼                                                                                   ║
║  if my_qty > 0  (order didn't fully fill):                                               ║
║      asks_by_tick[10150].push_back(incoming) ← REST the order in the book               ║
║      set_bit(ask_bitmap, 10150)              ← v3: mark tick as occupied                ║
║      order_location[5] = {Buy, 10150}        ← register for O(1) cancel later          ║
║      if 10150 > best_bid_tick:                                                           ║
║          best_bid_tick = 10150               ← update the top-of-book pointer           ║
║                                                                                          ║
╚══════════════════════════════════════════════════════════════════════════════════════════╝
                                     │
                                     ▼
╔══════════════════════════════════════════════════════════════════════════════════════════╗
║                        LAYER 4: PROGRAMS (EXECUTABLES)                                   ║
║              (Files that have a main() function and USE the engines above)              ║
╠══════════════════╦═══════════════════════╦═════════════════════════╦════════════════════╣
║  main.cpp        ║  cli_menu.cpp         ║  tests/                 ║  benchmark_3way.cpp║
║  ─────────────── ║  ─────────────────── ║  ─────────────────────  ║  ────────────────  ║
║  A scripted      ║  Interactive CLI.     ║  test_orderbook.cpp     ║  THE benchmark.    ║
║  step-by-step    ║  You manually type    ║    10 assert tests      ║  Runs v1, v2, v3   ║
║  demo of the     ║  in orders and see    ║    for v1 correctness   ║  on 1M identical   ║
║  orderbook.      ║  the book update.     ║                         ║  orders.           ║
║                  ║                       ║  test_orderbook_v3.cpp  ║  Tests NARROW and  ║
║  Uses v1         ║  Uses v1              ║    Same 10 tests        ║  WIDE spreads.     ║
║                  ║                       ║    for v3 correctness   ║  Uses v1+v2+v3     ║
╚══════════════════╩═══════════════════════╩═════════════════════════╩════════════════════╝
                                     │
                                     ▼
╔══════════════════════════════════════════════════════════════════════════════════════════╗
║                         LAYER 5: THE COMPILER COMMANDS                                   ║
╠══════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                          ║
║  g++  -std=c++17  -O3  -Wall -Wextra  -I include  [source files]  -o [output]           ║
║   │       │        │       │              │              │               │               ║
║   │       │        │       │              │              │               └── name of     ║
║   │       │        │       │              │              │                   executable  ║
║   │       │        │       │              │              └── which .cpp files to compile ║
║   │       │        │       │              └── look for #include files in /include folder ║
║   │       │        │       └── show all compiler warnings                               ║
║   │       │        └── maximum CPU optimization (loop unrolling, SIMD, inlining)        ║
║   │       └── use C++17 standard                                                        ║
║   └── the compiler                                                                       ║
║                                                                                          ║
╠══════════════════════════════════════════════════════════════════════════════════════════╣
║  COMMAND                                           WHAT IT BUILDS                        ║
║  ─────────────────────────────────────────────     ─────────────────────────────────    ║
║  g++ ... tests/test_orderbook.cpp                  Tests for v1                          ║
║          src/orderbook.cpp -o run_tests                                                  ║
║                                                                                          ║
║  g++ ... tests/test_orderbook_v3.cpp               Tests for v3                          ║
║          src/orderbook_v3.cpp -o run_tests_v3                                            ║
║                                                                                          ║
║  g++ ... cli_menu.cpp                              Interactive terminal app              ║
║          src/orderbook.cpp -o cli_menu                                                   ║
║                                                                                          ║
║  g++ ... benchmark_3way.cpp                        3-way benchmark ← USE THIS           ║
║          src/orderbook.cpp                         for resume numbers                    ║
║          src/orderbook_v2.cpp                                                            ║
║          src/orderbook_v3.cpp -o bench_3way                                              ║
╚══════════════════════════════════════════════════════════════════════════════════════════╝