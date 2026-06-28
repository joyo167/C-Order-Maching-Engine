#include "orderbook.h"
#include <iostream>

int main()
{
    orderbook book;

    // ---- Step 1: two resting asks from trader 100 ----
    book.submit_limit_order(1, /*trader*/100, Side::Sell, 101.0, 10);
    book.submit_limit_order(2, /*trader*/100, Side::Sell, 102.0, 5);

    // ---- Step 2: trader 100 submits a BUY at 101 ----
    // Same trader as order #1's seller. Self-trade prevention means
    // this should NOT match against order #1, even though the price
    // crosses - it should rest as a bid instead.
    book.submit_limit_order(3, /*trader*/100, Side::Buy, 101.0, 4);

    std::cout << "After step 2 (self-trade should be skipped):" << ln;
    book.printbook();
    std::cout << "Trades so far: " << book.get_trades().size() << " (expect 0)" << ln << ln;

    // ---- Step 3: trader 200 submits a BUY at 101 ----
    // Different trader - SHOULD match against order #1.
    book.submit_limit_order(4, /*trader*/200, Side::Buy, 101.0, 6);

    std::cout << "After step 3 (real trade should happen):" << ln;
    book.printbook();
    std::cout << "Trades so far: " << book.get_trades().size() << " (expect 1)" << ln << ln;

    // ---- Step 4: cancel the resting bid from step 2 ----
    bool canceled = book.cancel_order(3);
    std::cout << "Cancel order #3: " << (canceled ? "succeeded" : "failed") << " (expect succeeded)" << ln;

    // ---- Step 5: try to cancel an order that doesn't exist ----
    bool canceled_again = book.cancel_order(999);
    std::cout << "Cancel order #999: " << (canceled_again ? "succeeded" : "failed") << " (expect failed)" << ln << ln;

    std::cout << "Book before market order:" << ln;
    book.printbook();

    // ---- Step 6: a market buy that sweeps TWO price levels ----
    // Remaining book: ask #1 has qty=4 @101, ask #2 has qty=5 @102.
    // A market buy for 7 should consume all 4 from #1, then 3 more
    // from #2, leaving #2 with qty=2. It should NOT rest if unfilled.
    book.submit_market_order(5, /*trader*/300, Side::Buy, 7);

    std::cout << ln << "Book after market buy of 7 (should sweep both levels):" << ln;
    book.printbook();

    std::cout << ln << "All trades executed:" << ln;
    for (const auto& t : book.get_trades())
    {
        std::cout << "  BUY(" << t.buyorder_id << ")  x  SELL(" << t.sellorder_id
                  << ")  price=" << t.price << "  qty=" << t.quantity << ln;
    }

    return 0;
}