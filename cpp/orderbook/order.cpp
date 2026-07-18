#include "order.hpp"

namespace backtest {

Order Order::create(const Symbol& sym, Side s, OrderType t,
                    Quantity qty, Price limit_price) {
    Order o;
    o.id = IdGenerator::next_order_id();
    o.symbol = sym;
    o.side = s;
    o.type = t;
    o.price = limit_price;
    o.quantity = qty;
    o.remaining_quantity = qty;
    o.status = OrderStatus::New;
    return o;
}

void Order::apply_fill(Quantity fill_qty, Price fill_price) {
    double old_notional = avg_fill_price * filled_quantity.to_double();
    double fill_notional = fill_price.to_double() * fill_qty.to_double();

    filled_quantity += fill_qty;
    remaining_quantity -= fill_qty;

    if (filled_quantity.raw > 0) {
        avg_fill_price = (old_notional + fill_notional) / filled_quantity.to_double();
    }

    if (remaining_quantity.is_zero()) {
        status = OrderStatus::Filled;
    } else {
        status = OrderStatus::PartiallyFilled;
    }

    last_update_time = now_ns();
}

void Order::cancel() {
    if (is_active()) {
        status = OrderStatus::Cancelled;
        last_update_time = now_ns();
    }
}

void Order::reject(const std::string& /* reason */) {
    status = OrderStatus::Rejected;
    last_update_time = now_ns();
}

OrderRequest OrderRequest::market(const Symbol& sym, Side s, Quantity qty,
                                  const std::string& strategy) {
    OrderRequest req;
    req.symbol = sym;
    req.side = s;
    req.type = OrderType::Market;
    req.quantity = qty;
    req.strategy_id = strategy;
    return req;
}

OrderRequest OrderRequest::limit(const Symbol& sym, Side s, Quantity qty, Price price,
                                 const std::string& strategy) {
    OrderRequest req;
    req.symbol = sym;
    req.side = s;
    req.type = OrderType::Limit;
    req.quantity = qty;
    req.price = price;
    req.strategy_id = strategy;
    return req;
}

OrderRequest OrderRequest::ioc(const Symbol& sym, Side s, Quantity qty, Price price,
                               const std::string& strategy) {
    OrderRequest req;
    req.symbol = sym;
    req.side = s;
    req.type = OrderType::IOC;
    req.quantity = qty;
    req.price = price;
    req.tif = TimeInForce::IOC;
    req.strategy_id = strategy;
    return req;
}

}  // namespace backtest
