#pragma once
#include <cstdint>

// this structure will represent a trade

struct trade
{
    uint64_t buyorder_id;
    uint64_t sellorder_id;
    double price;
    uint64_t quantity;
};