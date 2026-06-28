#pragma once
#include <cstdint>

#define ln "\n"

// creating a user defined type for which side of market the order is on
enum class Side{ Buy, Sell};

// every order should have a unique id , which side of the market 
// price is in double because of the fractional stock prices
// quantity is >=0 
struct order
{
    uint64_t id;
    uint64_t trader_id;
    Side side;
    double price;
    uint64_t quantity;
};