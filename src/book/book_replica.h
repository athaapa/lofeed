#pragma once
#include "proto/itch.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>

struct RestingOrder {
    uint64_t order_ref;
    uint32_t quantity;
    uint32_t price;
    uint16_t stock_locate;
    char side;
};

struct BookStats {
    uint64_t duplicate_add = 0;
    uint64_t missing_delete = 0;
    uint64_t missing_cancel = 0;
    uint64_t missing_replace = 0;
    uint64_t missing_execute = 0;
    uint64_t over_cancel = 0;
    uint64_t over_execute = 0;
};

class BookReplica {
public:
    explicit BookReplica(size_t reserve_orders);
    void apply(const MarketEvent& event);

    size_t live_orders() const;
    const BookStats& stats() const;

private:
    std::unordered_map<uint64_t, RestingOrder> orders_;
    BookStats stats_;
};