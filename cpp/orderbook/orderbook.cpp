#include "orderbook.hpp"

namespace backtest {

void PriceLevel::add_order(Order* order) {
    order->prev_at_price = tail_;
    order->next_at_price = nullptr;

    if (tail_) {
        tail_->next_at_price = order;
    } else {
        head_ = order;
    }
    tail_ = order;

    total_quantity_ += order->remaining_quantity;
    ++order_count_;
}

void PriceLevel::remove_order(Order* order) {
    if (order->prev_at_price) {
        order->prev_at_price->next_at_price = order->next_at_price;
    } else {
        head_ = order->next_at_price;
    }

    if (order->next_at_price) {
        order->next_at_price->prev_at_price = order->prev_at_price;
    } else {
        tail_ = order->prev_at_price;
    }

    total_quantity_ -= order->remaining_quantity;
    --order_count_;

    order->prev_at_price = nullptr;
    order->next_at_price = nullptr;
}

void OrderBook::add_order(Order* order) {
    orders_[order->id] = order;

    if (order->side == Side::Buy) {
        bids_.add_order(order);
    } else {
        asks_.add_order(order);
    }
}

void OrderBook::remove_order(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return;

    Order* order = it->second;

    if (order->side == Side::Buy) {
        bids_.remove_order(order);
    } else {
        asks_.remove_order(order);
    }

    orders_.erase(it);
}

Price OrderBook::micro_price() const {
    if (bids_.empty() || asks_.empty()) return Price::zero();

    Quantity bid_qty = bids_.best_quantity();
    Quantity ask_qty = asks_.best_quantity();
    int64_t total = bid_qty.raw + ask_qty.raw;

    if (total == 0) return mid_price();

    // Micro price = (bid * ask_qty + ask * bid_qty) / (bid_qty + ask_qty)
    int64_t weighted = (bids_.best_price().raw * ask_qty.raw +
                       asks_.best_price().raw * bid_qty.raw) / total;
    return Price(weighted);
}

OrderBook::Snapshot OrderBook::get_snapshot(size_t depth) const {
    Snapshot snap;
    snap.symbol = symbol_;
    snap.timestamp = now_ns();
    snap.bids = bids_.get_depth(depth);
    snap.asks = asks_.get_depth(depth);
    return snap;
}

void OrderBook::clear() {
    orders_.clear();
    bids_ = OrderBookSide<true>();
    asks_ = OrderBookSide<false>();
}

}  // namespace backtest
