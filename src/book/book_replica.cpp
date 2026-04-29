#include "book_replica.h"

BookReplica::BookReplica(size_t reserve_orders) { orders_.reserve(reserve_orders); }

void BookReplica::apply(const MarketEvent& event) {
    auto it = orders_.find(event.order_ref);
    switch (event.type) {
    case MarketEvent::Type::Add: {
        if (it != orders_.end()) {
            stats_.duplicate_add++;
            break;
        }
        orders_.emplace(event.order_ref,
            RestingOrder {
                event.order_ref, event.quantity, event.price, event.stock_locate, event.side });
        break;
    }
    case MarketEvent::Type::Delete: {
        if (it == orders_.end()) {
            stats_.missing_delete++;
            break;
        }
        orders_.erase(it);
        break;
    }
    case MarketEvent::Type::Cancel: {
        if (it == orders_.end()) {
            stats_.missing_cancel++;
            break;
        }
        if (it->second.quantity < event.quantity) {
            stats_.over_cancel++;
            orders_.erase(it);
            break;
        } else if (it->second.quantity == event.quantity) {
            orders_.erase(it);
            break;
        } else {
            it->second.quantity -= event.quantity;
            break;
        }
    }
    case MarketEvent::Type::Replace: {
        if (it == orders_.end()) {
            stats_.missing_replace++;
            break;
        }
        RestingOrder old = it->second;
        orders_.erase(it);

        orders_.emplace(event.new_order_ref,
            RestingOrder {
                event.new_order_ref, event.quantity, event.price, old.stock_locate, old.side });
        break;
    }
    case MarketEvent::Type::Execute: {
        if (it == orders_.end()) {
            stats_.missing_execute++;
            break;
        }
        if (it->second.quantity < event.quantity) {
            stats_.over_execute++;
            orders_.erase(it);
            break;
        } else if (it->second.quantity == event.quantity) {
            orders_.erase(it);
            break;
        } else {
            it->second.quantity -= event.quantity;
            break;
        }
    }
    }
}

size_t BookReplica::live_orders() const { return orders_.size(); }

const BookStats& BookReplica::stats() const { return stats_; }