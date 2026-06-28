#include "orderbook.h"
#include <iostream>
#include <algorithm>
#include <limits>

void orderbook :: submit_limit_order(uint64_t id,uint64_t trader_id, Side side , double price, uint64_t quantity) 
{
    order incoming ={id,trader_id,side,price,quantity};

    if(side==Side:: Buy)
    {
        matchbuy(incoming);

        // if some quantity didnt get matched then place it in order book
        if(incoming.quantity>0)
        {
            bids[incoming.price].push_back(incoming);
        }
    }
    else
    {
        matchsell(incoming);

        // if some quantity didnt get matched then place it in order book
        if(incoming.quantity>0)
        {
            asks[incoming.price].push_back(incoming);
        }
    }
}
void orderbook :: submit_market_order(uint64_t id,uint64_t trader_id, Side side, uint64_t quantity)
{
     
    if(side==Side:: Buy)
    {
        order incoming = {id, trader_id, side,std::numeric_limits<double>::max(),quantity};
        matchbuy(incoming);
    }
    else
    {
        order incoming ={id, trader_id, side, 0.0, quantity};
        matchsell(incoming);
    }
}

bool orderbook :: cancel_order(uint64_t id)
{
    if(cancel_from_book(id, bids))
    {
        return true;
    }

    return cancel_from_book(id, asks);
}

void orderbook::matchbuy(order& incoming)
{
    // match orders as long as there is incoming quantity
    // and there are asks priced less than or equal to incoming price
    auto best_ask = asks.begin();

    while (incoming.quantity > 0 && best_ask != asks.end() && best_ask->first <= incoming.price)
    {
        auto resting = best_ask->second.begin();   // an ITERATOR, not a reference

        while (resting != best_ask->second.end() && incoming.quantity > 0)
        {
            if (resting->trader_id == incoming.trader_id)
            {
                ++resting;   // skip self-trade, try the next order at this same price
                continue;
            }

            uint64_t quantity_traded = std::min(incoming.quantity, resting->quantity);

            // important: trade happens at resting price, not incoming price
            trades.push_back(trade{incoming.id, resting->id, resting->price, quantity_traded});

            incoming.quantity -= quantity_traded;
            resting->quantity -= quantity_traded;

            if (resting->quantity == 0)
            {
                resting = best_ask->second.erase(resting); // erase returns the next valid iterator
            }
            else
            {
                ++resting;
            }
        }

        if (best_ask->second.empty())
        {
            best_ask = asks.erase(best_ask);
        }
        else
        {
            ++best_ask;   // <-- this line is the fix for the hang
        }
    }
}

void orderbook::matchsell(order& incoming)
{
    // match orders as long as there is incoming quantity
    // and there are bids priced greater than or equal to incoming price
    auto best_bid = bids.begin();

    while (incoming.quantity > 0 && best_bid != bids.end() && best_bid->first >= incoming.price)
    {
        auto resting = best_bid->second.begin();

        while (resting != best_bid->second.end() && incoming.quantity > 0)
        {
            if (resting->trader_id == incoming.trader_id)
            {
                ++resting;
                continue;
            }

            uint64_t quantity_traded = std::min(incoming.quantity, resting->quantity);

            // resting (the bid) is the BUYER here, incoming (the sell) is the SELLER -
            // note the order is flipped compared to matchbuy's Trade construction
            trades.push_back(trade{resting->id, incoming.id, resting->price, quantity_traded});

            incoming.quantity -= quantity_traded;
            resting->quantity -= quantity_traded;

            if (resting->quantity == 0)
            {
                resting = best_bid->second.erase(resting);
            }
            else
            {
                ++resting;
            }
        }

        if (best_bid->second.empty())
        {
            best_bid = bids.erase(best_bid);
        }
        else
        {
            ++best_bid;
        }
    }
}

void orderbook :: printbook() const 
{
    std :: cout <<"----------JOYO ORDERBOOK----------" << ln;
    std :: cout <<"ASKS (highest at top)" << ln;
    
    for(auto it=asks.rbegin(); it!=asks.rend();it++)
    {
        for(const auto &order :it->second)
        {
            std :: cout <<"["<<it->first << "] id=" <<order.id<<" trader="<<order.trader_id<<" qty= "<<order.quantity<< ln;
        }
    }

    std :: cout <<"-------------------" << ln;
    std :: cout <<"BIDS (highest at top) " << ln;

    for(const auto& bid : bids)
    {
        for(const auto&order : bid.second)
        {
            std :: cout <<"["<<bid.first << "] id=" <<order.id<<" trader="<<order.trader_id<<" qty= "<<order.quantity<< ln;
        }
    }
     std :: cout <<"-------------------" << ln;
}
