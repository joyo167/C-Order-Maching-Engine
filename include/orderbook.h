#pragma once

#include "order.h"
#include "trade.h"
#include<map>
#include<deque>
#include<vector>
#include<functional>

class orderbook 
{
    public :
    // submit limit order into the system 
    void submit_limit_order(uint64_t id,uint64_t trader_id, Side side, double price, uint64_t quantity);

    // submit market order into the system 
    void submit_market_order(uint64_t id,uint64_t trader_id, Side side, uint64_t quantity);
    
    // prints the orderbook state at any current state
    void printbook() const;

    // gives out the trades happened till current state
    const std:: vector<trade>& get_trades() const {return trades;}

    // cancel order from the system
    bool cancel_order(uint64_t id);

   

    private :

    // sorted decreasing by price because higher the bid more the chance of matching
    std::map<double, std::deque<order>,std::greater<double>> bids;

    // sorted increasing (as map does it by default) by price because lower the ask more the chance of matching
    std::map<double,std::deque<order>>asks;

    //stores all the trades
    std::vector<trade>trades;

    // internal logic to match the orders 
    void matchbuy(order & incoming);
    void matchsell(order & incoming);
    
    // have it for both type of maps , hence a single template 
    template<typename bookmap> bool cancel_from_book(uint64_t id, bookmap & book)
    {
        for(auto it =book.begin();it!=book.end();it++)
        {
            auto & level =it->second;
            auto orderIt = std::find_if(level.begin(), level.end(),
            [id](const order& o) { return o.id == id; });
            
            if (orderIt != level.end()) 
            {
                // removed the order 
                level.erase(orderIt);
                if (level.empty()) 
                {
                    // if no orders for this price , then erase the price
                    book.erase(it);
                }
                return true;
            }
        }
        return false;
    }
    
};