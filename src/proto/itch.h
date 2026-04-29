#pragma once
#include <cstdint>

struct __attribute__((packed)) AddOrder {
    uint8_t type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint8_t timestamp_ns[6];
    uint64_t order_ref;
    char side;
    uint32_t quantity;
    char stock_ticker[8];
    uint32_t price;
};

struct __attribute__((packed)) OrderDelete {
    uint8_t type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint8_t timestamp_ns[6];
    uint64_t order_ref;
};

struct __attribute__((packed)) OrderCancel {
    uint8_t type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint8_t timestamp_ns[6];
    uint64_t order_ref;
    uint32_t cancelled_shares;
};

struct __attribute__((packed)) OrderReplace {
    uint8_t type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint8_t timestamp_ns[6];
    uint64_t original_ref;
    uint64_t new_ref;
    uint32_t shares;
    uint32_t price;
};

struct __attribute__((packed)) AddOrderMPID {
    uint8_t type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint8_t timestamp_ns[6];
    uint64_t order_ref;
    char side;
    uint32_t quantity;
    char stock_ticker[8];
    uint32_t price;
    char attribution[4];
};

struct __attribute__((packed)) OrderExecuted {
    uint8_t type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint8_t timestamp_ns[6];
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_number;
};

struct __attribute__((packed)) OrderExecutedWithPrice {
    uint8_t type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint8_t timestamp_ns[6];
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_number;
    char printable;
    uint32_t execution_price;
};

struct MarketEvent {
    enum class Type : uint8_t { Add, Delete, Cancel, Replace, Execute };
    uint64_t order_ref;
    uint64_t new_order_ref;
    uint64_t timestamp_ns;

    uint32_t quantity;
    uint32_t price;

    uint16_t stock_locate;
    uint16_t tracking;

    Type type;
    char side;
    char stock_ticker[8];
    char attribution[4];
};

bool parse_market_event(const char* message, MarketEvent& event);

static_assert(sizeof(AddOrder) == 36, "ITCH message body size must be exactly 36 bytes");
static_assert(sizeof(OrderDelete) == 19, "ITCH message body size must be exactly 19 bytes");
static_assert(sizeof(OrderCancel) == 23, "ITCH message body size must be exactly 23 bytes");
static_assert(sizeof(OrderReplace) == 35, "ITCH message body size must be exactly 35 bytes");
static_assert(sizeof(AddOrderMPID) == 40, "ITCH message body size must be exactly 40 bytes");
static_assert(sizeof(OrderExecuted) == 31, "ITCH message body size must be exactly 31 bytes");
static_assert(
    sizeof(OrderExecutedWithPrice) == 36, "ITCH message body size must be exactly 36 bytes");