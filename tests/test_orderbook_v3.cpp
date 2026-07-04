#include "orderbook_v3.h"
#include <cassert>
#include <iostream>

// Each test is small and named after exactly one behavior. If an
// assert fails, the program crashes immediately and tells you the
// exact file/line - that's a real, automated correctness check,
// not "I read the printout and it looked right."

void test_partial_fill() {
    orderbook_v3 book;
    book.submit_limit_order(1, 100, Side::Sell, 101.0, 10);
    book.submit_limit_order(2, 200, Side::Buy, 101.0, 4);

    assert(book.get_trades().size() == 1);
    assert(book.get_trades()[0].quantity == 4);
    assert(book.get_trades()[0].price == 101.0);

    // Cancel what's left of order #1 to confirm it actually rested
    // with the correct remaining quantity (10 - 4 = 6).
    // We can't read quantity directly without a getter, so instead we
    // fully drain it and confirm exactly one more trade of qty 6 happens.
    book.submit_limit_order(3, 300, Side::Buy, 101.0, 6);
    assert(book.get_trades().size() == 2);
    assert(book.get_trades()[1].quantity == 6);

    std::cout << "test_partial_fill passed" << ln;
}

void test_full_fill_removes_order() {
    orderbook_v3 book;
    book.submit_limit_order(1, 100, Side::Sell, 50.0, 5);
    book.submit_limit_order(2, 200, Side::Buy, 50.0, 5);

    assert(book.get_trades().size() == 1);
    assert(book.get_trades()[0].quantity == 5);

    // Order #1 should be fully gone now. A new sell at the same id
    // would only be reachable if the old one didn't linger - prove
    // this indirectly: nothing should match a second buy at this
    // price unless we add a new resting sell first.
    book.submit_limit_order(3, 300, Side::Buy, 50.0, 1);
    assert(book.get_trades().size() == 1); // still 1 - no ask left to match

    std::cout << "test_full_fill_removes_order passed" << ln;
}

void test_cancel_removes_resting_order() {
    orderbook_v3 book;
    book.submit_limit_order(1, 100, Side::Sell, 75.0, 10);

    bool canceled = book.cancel_order(1);
    assert(canceled == true);

    // If order #1 is truly gone, a buy at 75 should find nothing to match.
    book.submit_limit_order(2, 200, Side::Buy, 75.0, 10);
    assert(book.get_trades().size() == 0);

    std::cout << "test_cancel_removes_resting_order passed" << ln;
}

void test_cancel_nonexistent_order_fails() {
    orderbook_v3 book;
    book.submit_limit_order(1, 100, Side::Sell, 75.0, 10);

    bool canceled = book.cancel_order(999);
    assert(canceled == false);

    std::cout << "test_cancel_nonexistent_order_fails passed" << ln;
}

void test_market_order_sweeps_multiple_levels() {
    orderbook_v3 book;
    book.submit_limit_order(1, 100, Side::Sell, 101.0, 4);
    book.submit_limit_order(2, 100, Side::Sell, 102.0, 5);

    book.submit_market_order(3, 200, Side::Buy, 7);

    assert(book.get_trades().size() == 2);
    assert(book.get_trades()[0].quantity == 4);
    assert(book.get_trades()[0].price == 101.0);
    assert(book.get_trades()[1].quantity == 3);
    assert(book.get_trades()[1].price == 102.0);

    std::cout << "test_market_order_sweeps_multiple_levels passed" << ln;
}

void test_market_order_does_not_rest_if_unfilled() {
    orderbook_v3 book;
    book.submit_limit_order(1, 100, Side::Sell, 101.0, 3);

    // Market buy wants 10, only 3 are available. The other 7 must
    // be discarded, NOT rested as a bid.
    book.submit_market_order(2, 200, Side::Buy, 10);
    assert(book.get_trades().size() == 1);
    assert(book.get_trades()[0].quantity == 3);

    // If the leftover 7 had incorrectly rested as a bid at some
    // price, a new sell order crossing that price would match it.
    // Use a very low sell price so it would cross almost any rested bid.
    book.submit_limit_order(3, 300, Side::Sell, 0.01, 1);
    assert(book.get_trades().size() == 1); // still 1 - nothing rested to match

    std::cout << "test_market_order_does_not_rest_if_unfilled passed" << ln;
}

void test_self_trade_is_prevented() {
    orderbook_v3 book;
    book.submit_limit_order(1, 100, Side::Sell, 101.0, 10);
    book.submit_limit_order(2, 100, Side::Buy, 101.0, 4); // same trader as #1

    assert(book.get_trades().size() == 0);

    // A different trader at the same price SHOULD match.
    book.submit_limit_order(3, 200, Side::Buy, 101.0, 4);
    assert(book.get_trades().size() == 1);
    assert(book.get_trades()[0].quantity == 4);

    std::cout << "test_self_trade_is_prevented passed" << ln;
}

void test_price_time_priority_among_equal_prices() {
    orderbook_v3 book;
    // Two sells at the SAME price, placed in this order.
    book.submit_limit_order(1, 100, Side::Sell, 101.0, 5); // placed first
    book.submit_limit_order(2, 200, Side::Sell, 101.0, 5); // placed second

    // A buy big enough to need both - the FIRST one placed must be
    // filled first (time priority within the same price level).
    book.submit_limit_order(3, 300, Side::Buy, 101.0, 8);

    assert(book.get_trades().size() == 2);
    assert(book.get_trades()[0].sellorder_id == 1); // order #1 matched FIRST
    assert(book.get_trades()[0].quantity == 5);
    assert(book.get_trades()[1].sellorder_id == 2); // order #2 matched SECOND
    assert(book.get_trades()[1].quantity == 3);

    std::cout << "test_price_time_priority_among_equal_prices passed" << ln;
}

void test_zero_quantity_order_is_noop() {
    orderbook_v3 book;
    // A zero-quantity order shouldn't rest on the book or produce a trade.
    book.submit_limit_order(1, 100, Side::Sell, 101.0, 0);

    // If it had incorrectly rested, this buy would find it and trade.
    book.submit_limit_order(2, 200, Side::Buy, 101.0, 5);
    assert(book.get_trades().size() == 0);

    std::cout << "test_zero_quantity_order_is_noop passed" << ln;
}

void test_cancel_after_partial_fill() {
    orderbook_v3 book;
    book.submit_limit_order(1, 100, Side::Sell, 100.0, 10);
    book.submit_limit_order(2, 200, Side::Buy, 100.0, 4); // order #1 now has 6 left

    assert(book.get_trades().size() == 1);
    assert(book.get_trades()[0].quantity == 4);

    bool canceled = book.cancel_order(1); // must still be cancelable with 6 remaining
    assert(canceled == true);

    // If it's truly gone, a new buy at 100 should find nothing.
    book.submit_limit_order(3, 300, Side::Buy, 100.0, 1);
    assert(book.get_trades().size() == 1); // unchanged

    std::cout << "test_cancel_after_partial_fill passed" << ln;
}

int main() {
    test_partial_fill();
    test_full_fill_removes_order();
    test_cancel_removes_resting_order();
    test_cancel_nonexistent_order_fails();
    test_market_order_sweeps_multiple_levels();
    test_market_order_does_not_rest_if_unfilled();
    test_self_trade_is_prevented();
    test_price_time_priority_among_equal_prices();
    test_zero_quantity_order_is_noop();
    test_cancel_after_partial_fill();

    std::cout << ln << "ALL TESTS PASSED" << ln;
    return 0;
}