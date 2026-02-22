# Low-Latency Backtesting & Execution Simulator: A Complete Finance Guide

## For Those New to Finance

This document explains how trading actually works - from the moment you decide to buy something to when money changes hands. No prior finance knowledge is assumed. By the end, you'll understand what this software simulates and why it matters to traders.

---

## Table of Contents

1. [The Big Picture: What Problem Does This Solve?](#the-big-picture)
2. [How Trading Actually Works](#how-trading-actually-works)
3. [The Order Book: Where Prices Come From](#the-order-book)
4. [Types of Orders](#types-of-orders)
5. [What Happens When You Click "Buy"](#what-happens-when-you-click-buy)
6. [The Hidden Costs of Trading](#the-hidden-costs-of-trading)
7. [Why Backtesting Matters](#why-backtesting-matters)
8. [Trading Strategies Explained](#trading-strategies-explained)
9. [Position Management and P&L](#position-management-and-pnl)
10. [The Complete Flow: From Idea to Profit](#the-complete-flow)
11. [Glossary of Trading Terms](#glossary)

---

## The Big Picture: What Problem Does This Solve? <a name="the-big-picture"></a>

Imagine you have a trading idea:

> "When Apple stock drops 2% in an hour, it usually bounces back. I should buy every time this happens."

**How do you know if this actually makes money?**

You could:
1. **Trade it live**: Risk real money to find out (expensive and scary)
2. **Backtest it**: Simulate the strategy on historical data (safe and cheap)

Our software is a **backtesting engine** - it replays historical market data and simulates what would have happened if you traded your strategy. But unlike simple backtests, it simulates:

- **The actual order book** (not just prices)
- **Order matching** (your orders affect the market)
- **Slippage** (you don't always get the price you want)
- **Latency** (there's a delay between your decision and execution)
- **Transaction costs** (trading isn't free)

This gives you a **realistic** estimate of strategy performance, not a fantasy.

---

## How Trading Actually Works <a name="how-trading-actually-works"></a>

Before understanding our simulator, let's understand how financial markets operate.

### The Old Days: The Trading Floor

In movies, you see traders shouting on a exchange floor:

```
"100 SHARES APPLE AT 150!"
"I'LL TAKE 'EM!"
```

Humans matched buyers and sellers through verbal negotiation.

### Today: Electronic Markets

Now, everything is electronic:

```
Your Computer → Your Broker → Exchange → Matching Engine → Counterparty
     |              |            |            |              |
   1ms           10ms         1ms         1μs          10ms
   
   Total: ~25 milliseconds from click to execution
```

**Key Participants**:

| Participant | Role | Example |
|-------------|------|---------|
| **Retail Trader** | Individual investor | You, buying Apple |
| **Institutional Investor** | Manages others' money | Vanguard buying for index funds |
| **Market Maker** | Provides liquidity | Citadel Securities quoting prices |
| **Broker** | Connects traders to exchanges | Fidelity, Robinhood |
| **Exchange** | Operates the marketplace | NYSE, NASDAQ |

### The Matching Engine: Heart of the Market

Every exchange runs a **matching engine** - software that matches buy and sell orders.

Our simulator includes a matching engine that works exactly like real exchanges.

---

## The Order Book: Where Prices Come From <a name="the-order-book"></a>

### What is an Order Book?

The **order book** is a list of all outstanding buy and sell orders for a stock.

```
┌─────────────────────────────────────────────────────────────┐
│                    APPLE (AAPL) ORDER BOOK                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  BIDS (Buy Orders)              ASKS (Sell Orders)          │
│  ─────────────────              ────────────────            │
│                                                             │
│  Price    Quantity              Price    Quantity           │
│  ──────   ────────              ──────   ────────           │
│                                 $150.05   500               │ ← Best Ask
│                                 $150.06   1,200             │
│                                 $150.07   800               │
│                                 $150.10   2,500             │
│                                                             │
│  $150.00   1,000               │ ← Best Bid                 │
│  $149.99   2,500                                            │
│  $149.98   800                                              │
│  $149.95   3,000                                            │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  Spread: $0.05   Mid Price: $150.025                        │
└─────────────────────────────────────────────────────────────┘
```

### Key Concepts

**Best Bid**: The highest price anyone is willing to pay ($150.00)
- If you want to sell immediately, you'll get this price

**Best Ask**: The lowest price anyone is willing to sell at ($150.05)
- If you want to buy immediately, you'll pay this price

**Spread**: The gap between best bid and best ask ($0.05)
- This is the "cost" of immediacy
- Tighter spreads = more liquid markets

**Mid Price**: Average of best bid and best ask ($150.025)
- Often used as the "fair" price

**Depth**: How much volume exists at each price level
- More depth = more liquidity = easier to trade large sizes

### How the Order Book Changes

The order book is constantly changing as orders arrive and execute:

```
Time 10:00:00.001 - New buy order: 500 shares @ $150.00
                    → Adds to bid at $150.00

Time 10:00:00.002 - New sell order: 200 shares @ $150.00 (market order)
                    → Matches with bid at $150.00
                    → 200 shares trade, bid reduced to 800

Time 10:00:00.003 - Cancel order: Remove 300 shares from $149.99 bid
                    → Bid at $149.99 reduced from 2,500 to 2,200
```

Our simulator processes these events thousands of times per second, just like real exchanges.

---

## Types of Orders <a name="types-of-orders"></a>

### Market Order: "Buy Now, Whatever the Price"

```
Order: BUY 100 shares of AAPL at MARKET

Order Book Before:
  Best Ask: $150.05 × 500

Execution:
  Filled 100 shares @ $150.05

Result: You bought immediately but paid the "ask" price
```

**Pros**: Guaranteed execution
**Cons**: No price control, can be expensive in volatile markets

### Limit Order: "Buy Only at My Price or Better"

```
Order: BUY 100 shares of AAPL LIMIT $150.00

Order Book Before:
  Best Ask: $150.05 (higher than your limit)

Result: Your order RESTS on the book as a new bid at $150.00
        You wait until someone is willing to sell at $150.00 or lower
```

**Pros**: Price control
**Cons**: Might not execute

### Time-in-Force: How Long Orders Stay Active

| Type | Meaning | Use Case |
|------|---------|----------|
| **GTC** (Good Till Canceled) | Stays until filled or you cancel | "I want this price, will wait" |
| **IOC** (Immediate or Cancel) | Fill what you can now, cancel rest | "Give me liquidity now" |
| **FOK** (Fill or Kill) | Fill entirely now, or cancel entirely | "I need all or nothing" |

```
IOC Example:
Order: BUY 1,000 shares LIMIT $150.05 IOC

Order Book:
  Ask $150.05: 300 shares available

Result: 
  Filled 300 shares @ $150.05
  Remaining 700 shares CANCELED (not enough liquidity)
```

### Our Simulator Supports All Order Types

The matching engine in our software handles:
- Market orders
- Limit orders
- All time-in-force variations
- Order cancellations
- Partial fills

---

## What Happens When You Click "Buy" <a name="what-happens-when-you-click-buy"></a>

Let's trace what happens when you buy 1,000 shares of Apple:

### Step 1: Order Creation (Your Computer)

```
You click "Buy 1,000 AAPL"
Your trading software creates an order:
{
  symbol: "AAPL",
  side: "BUY",
  quantity: 1000,
  order_type: "MARKET",
  timestamp: 2024-01-15 10:30:00.123456789
}
```

### Step 2: Order Routing (Your Broker)

```
Your broker receives the order
Broker checks:
  ✓ Do you have enough money?
  ✓ Is this order legal?
  ✓ Which exchange has best price?

Broker routes to NASDAQ (best ask is there)
```

### Step 3: Order Matching (Exchange)

```
NASDAQ Matching Engine receives order

Order Book Before:
  Ask $150.05: 500 shares (Market Maker A)
  Ask $150.06: 300 shares (Market Maker B)  
  Ask $150.07: 400 shares (Market Maker C)

Your 1,000 share buy matches AGAINST the asks:
  Fill #1: 500 @ $150.05 (clears first level)
  Fill #2: 300 @ $150.06 (clears second level)
  Fill #3: 200 @ $150.07 (partially fills third level)

Order Book After:
  Ask $150.07: 200 shares (remaining from Market Maker C)
```

### Step 4: Trade Reporting

```
Exchange reports trades:
  Trade 1: AAPL 500 @ $150.05
  Trade 2: AAPL 300 @ $150.06
  Trade 3: AAPL 200 @ $150.07

Your average price: (500×150.05 + 300×150.06 + 200×150.07) / 1000
                  = $150.056

You wanted to buy at "market" (~$150.05)
You actually paid $150.056
The extra $0.006/share = $6 total slippage
```

### Step 5: Settlement (T+1)

```
Next business day:
  - $150,056 leaves your account
  - 1,000 AAPL shares appear in your account
  - Market makers receive the cash
```

**Our simulator models all of this** - including the slippage you experience when your order "walks the book."

---

## The Hidden Costs of Trading <a name="the-hidden-costs-of-trading"></a>

Trading isn't free. Understanding costs is crucial for realistic backtesting.

### 1. Explicit Costs: Commissions and Fees

```
Commission: $0.005 per share (institutional)
           or $0 for retail (but broker sells your order flow)

Regulatory fees: ~$0.0001 per share
Exchange fees: ~$0.0003 per share

For 1,000 shares:
  Commission: $5.00
  Fees: $0.40
  Total explicit: $5.40
```

### 2. Implicit Costs: Spread and Slippage

#### The Spread Cost

```
You buy at $150.05 (ask)
Fair value is $150.025 (mid)
Spread cost: $0.025 per share × 1,000 = $25
```

Every time you cross the spread, you "pay" half the spread.

#### Slippage: Impact of Your Order

```
Ideal execution: 1,000 @ $150.05
Actual execution: 1,000 @ $150.056 (walked the book)
Slippage: $0.006 × 1,000 = $6
```

Large orders move the market against you.

### 3. Market Impact: You Move the Price

When big traders buy, they push prices up:

```
Before your order: Mid price = $150.025
You start buying...
  - Other traders see buying pressure
  - Market makers widen spreads
  - Prices move against you
After your order: Mid price = $150.08

Permanent impact: $0.055 per share
On 10,000 shares = $550 cost
```

### 4. Timing Cost: Opportunity Cost

```
You decide to buy at 10:00 AM, price is $150.00
You finish buying at 10:30 AM
Price drifted to $150.20 while you were executing

Timing cost: $0.20 × size
```

### How Our Simulator Models Costs

```cpp
// Transaction cost configuration
struct TransactionCostConfig {
    double maker_fee_bps = 0.0;   // Fee for providing liquidity
    double taker_fee_bps = 1.0;   // Fee for taking liquidity (1 basis point)
    double fixed_cost = 0.0;      // Per-trade fixed cost
    double min_cost = 0.0;        // Minimum cost per trade
};

// Slippage configuration
struct SlippageConfig {
    double base_slippage_bps = 0.0;
    double volume_impact_bps = 0.0;   // Increases with order size
    bool use_market_impact = true;     // Square-root impact model
};
```

---

## Why Backtesting Matters <a name="why-backtesting-matters"></a>

### The Problem with Paper Trading

**Paper trading** (simulated trading with fake money) has flaws:

1. **No market impact**: You assume infinite liquidity
2. **Perfect execution**: You always get the price you want
3. **No slippage**: Reality is messier
4. **Emotional difference**: Fake money doesn't trigger fear/greed

### What Backtesting Provides

Good backtesting answers:

1. **Would this strategy have made money?**
2. **How much risk would I have taken?**
3. **What's the worst drawdown I should expect?**
4. **How sensitive is the strategy to transaction costs?**
5. **Does it work across different market conditions?**

### The Backtesting Danger: Overfitting

**Overfitting**: Finding patterns that only existed in the past

```
Bad backtest process:
1. Look at data
2. Notice: "When RSI < 20 and it's a Tuesday, stock goes up"
3. Backtest this rule → Amazing returns!
4. Trade it live → Loses money

Why? You found a coincidence, not a real pattern.
```

**How to avoid overfitting**:
- Use out-of-sample data (don't peek!)
- Keep strategies simple
- Test across multiple time periods
- Be skeptical of amazing results

### What Makes Our Simulator Different

Most backtests use **daily closing prices**:

```
Simple backtest:
Day 1: Close = $150.00, Signal: BUY
Day 2: Assume you bought at $150.00

Reality: You couldn't buy at exactly $150.00
```

Our simulator uses **tick-by-tick data**:

```
Realistic backtest:
10:30:00.001 - Signal: BUY
10:30:00.002 - Submit order
10:30:00.012 - Order reaches exchange (10ms latency)
10:30:00.013 - Partial fill 200 @ $150.01
10:30:00.014 - Partial fill 300 @ $150.02
10:30:00.015 - Partial fill 500 @ $150.03
10:30:00.015 - Order complete, avg price $150.022

Slippage: $0.022/share = $22 on 1,000 shares
```

---

## Trading Strategies Explained <a name="trading-strategies-explained"></a>

Our simulator comes with four example strategies:

### Strategy 1: VWAP Execution

**Goal**: Buy a large amount without moving the market

**VWAP** = Volume-Weighted Average Price

```
Scenario: Buy 100,000 shares of AAPL over 1 hour

Bad approach: Market order for 100,000
  → Moves price significantly
  → Pay $150.50 average instead of $150.00
  → Cost: $50,000 in slippage

VWAP approach:
  - Split into 20 slices of 5,000 shares
  - Execute each slice proportional to historical volume
  - More volume at open and close, less at midday
  
  Expected volume distribution:
  9:30-10:00: 15% → Execute 15,000 shares
  10:00-11:00: 10% → Execute 10,000 shares
  11:00-12:00: 8% → Execute 8,000 shares
  ...
  
Result: Average price close to market VWAP
        Much less market impact
```

### Strategy 2: Mean Reversion

**Idea**: "What goes down must come up" (sometimes)

```
Observation: Stock prices tend to revert to their average

Strategy:
1. Calculate 20-period moving average: $150.00
2. Calculate current price: $145.00
3. Calculate Z-score: (145 - 150) / std_dev = -2.5

If Z-score < -2.0: BUY (price is "too low")
If Z-score > +2.0: SELL (price is "too high")
If |Z-score| < 0.5: Close position (price is "normal")

The bet: Extreme moves are temporary
```

**When it works**: Range-bound markets
**When it fails**: Trending markets (catching falling knives)

### Strategy 3: Market Making

**Idea**: Profit from the bid-ask spread by providing liquidity

```
Current market:
  Bid: $150.00
  Ask: $150.10
  Spread: $0.10

Market maker quotes:
  My Bid: $150.03 (better than market bid)
  My Ask: $150.07 (better than market ask)
  My Spread: $0.04

If someone sells to me at $150.03
And later someone buys from me at $150.07
Profit: $0.04 per share

The catch: Inventory risk
  - If price drops after I buy, I lose
  - Must manage position to stay "neutral"
```

**Inventory Management**:
```
If I'm long (bought more than sold):
  → Lower my bid (less eager to buy more)
  → Lower my ask (more eager to sell)
  
This "skews" quotes to push inventory back to zero
```

### Strategy 4: Momentum

**Idea**: "Trend is your friend" - winning stocks keep winning

```
Observation: Stocks that went up tend to keep going up

Strategy:
1. Calculate cumulative return over 10 periods
2. If return > +0.5%: BUY (positive momentum)
3. If return < -0.5%: SELL (negative momentum)
4. If position exists and momentum reverses: CLOSE

The bet: Trends persist
```

**When it works**: Trending markets
**When it fails**: Choppy, range-bound markets

---

## Position Management and P&L <a name="position-management-and-pnl"></a>

### What is a Position?

A **position** is your current holding in a security:

```
Position States:

FLAT (no position):
  Shares: 0
  P&L: $0

LONG (you own shares):
  Shares: +1,000
  Entry price: $150.00
  Current price: $152.00
  Unrealized P&L: +$2,000

SHORT (you owe shares):
  Shares: -1,000
  Entry price: $150.00
  Current price: $152.00
  Unrealized P&L: -$2,000 (price went against you)
```

### How P&L Works

**Unrealized P&L**: Paper profit/loss on open positions
- Changes every time the price moves
- Not "real" until you close the position

**Realized P&L**: Actual profit/loss from closed trades
- Locked in when you sell
- This is real money

```
Trade Sequence:

1. BUY 1,000 @ $150.00
   Position: +1,000
   Realized P&L: $0
   Unrealized P&L: $0

2. Price moves to $155.00
   Position: +1,000
   Realized P&L: $0
   Unrealized P&L: +$5,000

3. SELL 500 @ $155.00
   Position: +500
   Realized P&L: +$2,500 (sold half at $5 profit)
   Unrealized P&L: +$2,500 (remaining half)

4. Price drops to $148.00
   Position: +500
   Realized P&L: +$2,500 (unchanged - already locked in)
   Unrealized P&L: -$1,000 (remaining at loss vs entry)

5. SELL 500 @ $148.00
   Position: 0 (flat)
   Realized P&L: +$2,500 + (-$1,000) = +$1,500
   Unrealized P&L: $0
```

### Average Entry Price

When you build a position over multiple trades:

```
Trade 1: BUY 500 @ $150.00
Trade 2: BUY 300 @ $152.00
Trade 3: BUY 200 @ $148.00

Average Entry Price = (500×150 + 300×152 + 200×148) / 1000
                    = $150.10

Total Position: 1,000 shares @ $150.10 average
```

### Performance Metrics

Our simulator tracks comprehensive statistics:

```
┌─────────────────────────────────────────────────────────────┐
│              STRATEGY PERFORMANCE REPORT                     │
├─────────────────────────────────────────────────────────────┤
│ Period: 2023-01-01 to 2023-12-31                            │
│                                                             │
│ RETURNS                                                     │
│ Total Return:        +15.7%                                 │
│ Annualized Return:   +15.7%                                 │
│ Risk-Free Rate:      +5.0%                                  │
│                                                             │
│ RISK METRICS                                                │
│ Volatility (annualized):  12.3%                             │
│ Sharpe Ratio:             0.87 (good)                       │
│ Max Drawdown:             -8.2%                             │
│ Longest Drawdown:         23 days                           │
│                                                             │
│ TRADING STATISTICS                                          │
│ Total Trades:             1,247                             │
│ Winning Trades:           687 (55.1%)                       │
│ Average Win:              $234                              │
│ Average Loss:             -$178                             │
│ Profit Factor:            1.44                              │
│                                                             │
│ COSTS                                                       │
│ Total Commissions:        $6,235                            │
│ Total Slippage:           $12,450                           │
│ Costs as % of Gross P&L:  11.2%                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Key Metrics Explained**:

| Metric | What It Means | Good Value |
|--------|---------------|------------|
| **Sharpe Ratio** | Return per unit of risk | > 1.0 is good, > 2.0 is excellent |
| **Max Drawdown** | Worst peak-to-trough loss | Lower is better, < 20% typical |
| **Win Rate** | % of trades that profit | > 50% for momentum, can be < 50% if wins > losses |
| **Profit Factor** | Gross profits / Gross losses | > 1.5 is solid |

---

## The Complete Flow: From Idea to Profit <a name="the-complete-flow"></a>

Let's trace a complete backtesting workflow:

### Phase 1: Strategy Development

```
Hypothesis: "Tech stocks bounce after 3 consecutive down days"

Define rules:
  Entry: If stock is down 3 days in a row, BUY at open on day 4
  Exit: Sell after 5 days OR if position is down 2%
  Size: Invest 10% of portfolio per trade
```

### Phase 2: Data Preparation

```
Load historical data:
  - Symbol: AAPL
  - Period: 2020-01-01 to 2023-12-31
  - Data type: Tick-by-tick (every trade)
  
Data includes:
  - Timestamp (nanosecond precision)
  - Bid price and size
  - Ask price and size
  - Last trade price and size
```

### Phase 3: Backtest Configuration

```python
config = BacktestConfig(
    initial_capital=1_000_000,  # Start with $1M
    
    # Transaction costs
    taker_fee_bps=1.0,     # 1 basis point per trade
    maker_fee_bps=-0.2,    # 0.2 bp rebate for providing liquidity
    
    # Slippage model
    slippage_model="volume_impact",
    impact_coefficient=0.1,  # 10 bps per 1% of volume
    
    # Latency simulation  
    order_latency_ms=10,   # 10ms to reach exchange
    market_data_latency_ms=5,  # 5ms to receive data
)
```

### Phase 4: Run Simulation

```
Simulation Timeline:

2020-03-16: COVID crash begins
  10:30:00 - Signal: AAPL down 3 days, BUY
  10:30:00.010 - Order submitted
  10:30:00.020 - Order reaches exchange
  10:30:00.021 - Fill: 500 @ $242.15
  10:30:00.022 - Fill: 300 @ $242.17
  10:30:00.023 - Fill: 200 @ $242.20
  Average fill: $242.17 (wanted $242.10, slippage $0.07)

  Position: +1,000 AAPL @ $242.17
  
2020-03-17: Market continues falling
  Price drops to $230.00
  Unrealized P&L: -$12,170
  Stop loss triggered at 2%? NO (down 5% but rule says 2% of portfolio, not position)
  
2020-03-23: Market bottom
  Price: $224.00
  Unrealized P&L: -$18,170
  
2020-03-24: Day 5, time to exit
  10:30:00 - Signal: SELL (5-day hold complete)
  Order fills at $236.50
  
  Trade P&L: ($236.50 - $242.17) × 1,000 = -$5,670
  Commission: $10
  Total: -$5,680
```

### Phase 5: Analyze Results

```
Backtest Summary (4 years):

Total trades: 156
Profitable: 89 (57%)
Unprofitable: 67 (43%)

Gross profit: $234,500
Gross loss: -$156,200
Net profit: $78,300

Return: +7.8%
Annualized: +1.9%
Sharpe: 0.31

Verdict: Strategy is marginally profitable but doesn't beat
         risk-free rate (5%). Not worth trading.
```

### Phase 6: Iterate and Improve

```
Analysis of losing trades:
  - Most losses occurred during strong downtrends
  - "Bounce" didn't happen when market was crashing

Improvement ideas:
  1. Add trend filter: Only trade when 50-day MA is rising
  2. Reduce position size during high volatility
  3. Tighter stop loss

Re-run backtest with improvements...
```

---

## Glossary of Trading Terms <a name="glossary"></a>

| Term | Definition |
|------|------------|
| **Ask** | Price at which sellers offer to sell |
| **Backtesting** | Testing a strategy on historical data |
| **Bid** | Price at which buyers offer to buy |
| **Commission** | Fee paid to broker for executing trade |
| **Drawdown** | Decline from peak portfolio value |
| **Exchange** | Marketplace where securities trade |
| **Fill** | Execution of an order |
| **FIFO** | First In, First Out (order matching priority) |
| **Latency** | Time delay in communication |
| **Limit Order** | Order to buy/sell at a specific price |
| **Liquidity** | Ability to trade without moving price |
| **Long** | Owning shares (bullish position) |
| **Market Impact** | Price movement caused by your trading |
| **Market Maker** | Firm providing constant bid/ask quotes |
| **Market Order** | Order to trade immediately at best price |
| **Matching Engine** | System that matches buy and sell orders |
| **Order Book** | List of all open orders |
| **P&L** | Profit and Loss |
| **Position** | Current holdings in a security |
| **Realized P&L** | Profit/loss from closed positions |
| **Short** | Selling borrowed shares (bearish position) |
| **Slippage** | Difference between expected and actual price |
| **Spread** | Difference between bid and ask |
| **Tick** | Single price update |
| **Unrealized P&L** | Paper profit/loss on open positions |
| **VWAP** | Volume-Weighted Average Price |

---

## Why This Simulator Matters

### For Individual Traders

- Test ideas before risking real money
- Understand true trading costs
- Build confidence in strategies
- Avoid costly mistakes

### For Institutions

- Evaluate execution algorithms
- Optimize trading schedules
- Measure transaction cost analysis (TCA)
- Regulatory compliance testing

### For Quantitative Researchers

- Rapid strategy prototyping
- Factor analysis
- Risk model validation
- Market microstructure research

---

## The Bottom Line

Our backtesting simulator answers the fundamental question every trader asks:

> **"Will my strategy actually make money in the real world?"**

By simulating:
- Real order book dynamics
- Realistic execution with latency
- Slippage and market impact
- Transaction costs and fees
- Position and P&L tracking

We provide answers you can trust - not fantasy returns from perfect execution, but realistic estimates of what to expect when real money is on the line.

---

*This document explains the financial concepts. For technical implementation details, see [DESIGN.md](DESIGN.md).*
