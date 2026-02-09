#pragma once

#include "core/types.hpp"
#include "orderbook/order.hpp"
#include <variant>
#include <string>
#include <memory>

namespace backtest {

// ============================================================================
// Event Base
// ============================================================================
struct EventBase {
    Timestamp timestamp;
    EventType type;
    
    EventBase() : timestamp(0), type(EventType::MarketData) {}
    EventBase(Timestamp ts, EventType t) : timestamp(ts), type(t) {}
    
    virtual ~EventBase() = default;
};

// ============================================================================
// Market Data Event
// ============================================================================
struct MarketDataEvent : EventBase {
    Tick tick;
    
    MarketDataEvent() : EventBase(0, EventType::MarketData) {}
    explicit MarketDataEvent(const Tick& t) 
        : EventBase(t.timestamp, EventType::MarketData), tick(t) {}
};

// ============================================================================
// Order Events
// ============================================================================
struct OrderSubmitEvent : EventBase {
    OrderRequest request;
    
    OrderSubmitEvent() : EventBase(0, EventType::OrderSubmit) {}
    OrderSubmitEvent(Timestamp ts, const OrderRequest& req)
        : EventBase(ts, EventType::OrderSubmit), request(req) {}
};

struct OrderCancelEvent : EventBase {
    CancelRequest request;
    
    OrderCancelEvent() : EventBase(0, EventType::OrderCancel) {}
    OrderCancelEvent(Timestamp ts, const CancelRequest& req)
        : EventBase(ts, EventType::OrderCancel), request(req) {}
};

struct OrderFillEvent : EventBase {
    Fill fill;
    double transaction_cost;
    
    OrderFillEvent() : EventBase(0, EventType::OrderFill), transaction_cost(0) {}
    OrderFillEvent(Timestamp ts, const Fill& f, double cost = 0)
        : EventBase(ts, EventType::OrderFill), fill(f), transaction_cost(cost) {}
};

struct OrderRejectEvent : EventBase {
    OrderId order_id;
    std::string reason;
    
    OrderRejectEvent() : EventBase(0, EventType::OrderReject), order_id(0) {}
    OrderRejectEvent(Timestamp ts, OrderId id, const std::string& r)
        : EventBase(ts, EventType::OrderReject), order_id(id), reason(r) {}
};

// ============================================================================
// Trade Event (public trade feed)
// ============================================================================
struct TradeEvent : EventBase {
    Symbol symbol;
    Price price;
    Quantity quantity;
    Side aggressor_side;
    
    TradeEvent() : EventBase(0, EventType::Trade), aggressor_side(Side::Buy) {}
    TradeEvent(Timestamp ts, const Symbol& sym, Price p, Quantity q, Side agg)
        : EventBase(ts, EventType::Trade), symbol(sym), price(p), 
          quantity(q), aggressor_side(agg) {}
};

// ============================================================================
// Position Update Event
// ============================================================================
struct PositionUpdateEvent : EventBase {
    Symbol symbol;
    Quantity position;          // Current position (negative for short)
    double avg_entry_price;     // Average entry price
    double unrealized_pnl;      // Unrealized P&L
    double realized_pnl;        // Realized P&L
    
    PositionUpdateEvent() : EventBase(0, EventType::PositionUpdate),
        avg_entry_price(0), unrealized_pnl(0), realized_pnl(0) {}
};

// ============================================================================
// Session Events
// ============================================================================
struct SessionStartEvent : EventBase {
    Timestamp session_start;
    Timestamp session_end;
    
    SessionStartEvent() : EventBase(0, EventType::SessionStart),
        session_start(0), session_end(0) {}
    SessionStartEvent(Timestamp ts, Timestamp start, Timestamp end)
        : EventBase(ts, EventType::SessionStart), 
          session_start(start), session_end(end) {}
};

struct SessionEndEvent : EventBase {
    double day_pnl;
    double total_volume;
    int total_trades;
    
    SessionEndEvent() : EventBase(0, EventType::SessionEnd),
        day_pnl(0), total_volume(0), total_trades(0) {}
    SessionEndEvent(Timestamp ts, double pnl, double vol, int trades)
        : EventBase(ts, EventType::SessionEnd),
          day_pnl(pnl), total_volume(vol), total_trades(trades) {}
};

struct EndOfDayEvent : EventBase {
    Timestamp next_session_start;
    
    EndOfDayEvent() : EventBase(0, EventType::EndOfDay), next_session_start(0) {}
    EndOfDayEvent(Timestamp ts, Timestamp next)
        : EventBase(ts, EventType::EndOfDay), next_session_start(next) {}
};

// ============================================================================
// Event Variant - Type-safe union of all events
// ============================================================================
using Event = std::variant<
    MarketDataEvent,
    OrderSubmitEvent,
    OrderCancelEvent,
    OrderFillEvent,
    OrderRejectEvent,
    TradeEvent,
    PositionUpdateEvent,
    SessionStartEvent,
    SessionEndEvent,
    EndOfDayEvent
>;

// ============================================================================
// Event Utilities
// ============================================================================
inline Timestamp get_event_timestamp(const Event& event) {
    return std::visit([](const auto& e) { return e.timestamp; }, event);
}

inline EventType get_event_type(const Event& event) {
    return std::visit([](const auto& e) { return e.type; }, event);
}

// Event ordering by timestamp
struct EventComparator {
    bool operator()(const Event& a, const Event& b) const {
        return get_event_timestamp(a) > get_event_timestamp(b);  // Min-heap
    }
};

}  // namespace backtest
