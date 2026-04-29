#include "itch.h"
#include <arpa/inet.h>
#include <cstring>

static uint64_t decode_u48_be(const uint8_t bytes[6]) {
    uint64_t value = 0;
    for (int i = 0; i < 6; i++) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

bool parse_market_event(const char* message, MarketEvent& event) {
    event = {};

    if (message[0] == 'A' || message[0] == 'F') {
        event.type = MarketEvent::Type::Add;
    } else if (message[0] == 'D') {
        event.type = MarketEvent::Type::Delete;
    } else if (message[0] == 'X') {
        event.type = MarketEvent::Type::Cancel;
    } else if (message[0] == 'U') {
        event.type = MarketEvent::Type::Replace;
    } else if (message[0] == 'C' || message[0] == 'E') {
        event.type = MarketEvent::Type::Execute;
    } else {
        return false;
    }

    switch (event.type) {
    case MarketEvent::Type::Add: {
        if (message[0] == 'A') {
            const auto* add_order = reinterpret_cast<const AddOrder*>(message);
            event.timestamp_ns = decode_u48_be(add_order->timestamp_ns);
            event.order_ref = __builtin_bswap64(add_order->order_ref);
            event.quantity = ntohl(add_order->quantity);
            event.price = ntohl(add_order->price);
            event.stock_locate = ntohs(add_order->stock_locate);
            event.tracking = ntohs(add_order->tracking);
            event.side = add_order->side;
            std::memcpy(event.stock_ticker, add_order->stock_ticker, 8);
        } else {
            const auto* add_order = reinterpret_cast<const AddOrderMPID*>(message);
            event.timestamp_ns = decode_u48_be(add_order->timestamp_ns);
            event.order_ref = __builtin_bswap64(add_order->order_ref);
            event.quantity = ntohl(add_order->quantity);
            event.price = ntohl(add_order->price);
            event.stock_locate = ntohs(add_order->stock_locate);
            event.tracking = ntohs(add_order->tracking);
            event.side = add_order->side;
            std::memcpy(event.stock_ticker, add_order->stock_ticker, 8);
            std::memcpy(event.attribution, add_order->attribution, 4);
        }
        break;
    }
    case MarketEvent::Type::Delete: {
        const auto* delete_order = reinterpret_cast<const OrderDelete*>(message);
        event.order_ref = __builtin_bswap64(delete_order->order_ref);
        event.timestamp_ns = decode_u48_be(delete_order->timestamp_ns);
        event.stock_locate = ntohs(delete_order->stock_locate);
        event.tracking = ntohs(delete_order->tracking);
        break;
    }
    case MarketEvent::Type::Cancel: {
        const auto* cancel_order = reinterpret_cast<const OrderCancel*>(message);
        event.order_ref = __builtin_bswap64(cancel_order->order_ref);
        event.timestamp_ns = decode_u48_be(cancel_order->timestamp_ns);
        event.quantity = ntohl(cancel_order->cancelled_shares);
        event.stock_locate = ntohs(cancel_order->stock_locate);
        event.tracking = ntohs(cancel_order->tracking);
        break;
    }
    case MarketEvent::Type::Replace: {
        const auto* replace_order = reinterpret_cast<const OrderReplace*>(message);
        event.order_ref = __builtin_bswap64(replace_order->original_ref);
        event.new_order_ref = __builtin_bswap64(replace_order->new_ref);
        event.timestamp_ns = decode_u48_be(replace_order->timestamp_ns);
        event.quantity = ntohl(replace_order->shares);
        event.price = ntohl(replace_order->price);
        event.stock_locate = ntohs(replace_order->stock_locate);
        event.tracking = ntohs(replace_order->tracking);
        break;
    }
    case MarketEvent::Type::Execute: {
        if (message[0] == 'E') {
            const auto* execute_order = reinterpret_cast<const OrderExecuted*>(message);
            event.order_ref = __builtin_bswap64(execute_order->order_ref);
            event.timestamp_ns = decode_u48_be(execute_order->timestamp_ns);
            event.quantity = ntohl(execute_order->executed_shares);
            event.stock_locate = ntohs(execute_order->stock_locate);
            event.tracking = ntohs(execute_order->tracking);
            event.price = 0;
        } else {
            const auto* execute_order = reinterpret_cast<const OrderExecutedWithPrice*>(message);
            event.order_ref = __builtin_bswap64(execute_order->order_ref);
            event.timestamp_ns = decode_u48_be(execute_order->timestamp_ns);
            event.quantity = ntohl(execute_order->executed_shares);
            event.stock_locate = ntohs(execute_order->stock_locate);
            event.tracking = ntohs(execute_order->tracking);
            event.price = ntohl(execute_order->execution_price);
        }
        break;
    }
    }
    return true;
}