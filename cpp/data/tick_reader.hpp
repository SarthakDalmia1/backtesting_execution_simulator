#pragma once

#include "core/types.hpp"
#include "data/market_data_feed.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

namespace backtest {

// ============================================================================
// CSV Tick Reader
// ============================================================================
// Reads tick data from CSV files
// Expected format: timestamp,symbol,bid,ask,bid_size,ask_size,last,last_size,volume
class CSVTickReader : public MarketDataFeed {
public:
    explicit CSVTickReader(const std::string& filename, char delimiter = ',')
        : filename_(filename), delimiter_(delimiter), line_number_(0), is_open_(false) {}
    
    ~CSVTickReader() override;

    bool open();

    bool next_tick(Tick& tick) override;

    void reset() override;
    
    bool has_data() const override {
        return is_open_ && file_.good() && !file_.eof();
    }
    
    size_t lines_read() const { return line_number_; }

private:
    bool parse_line(const std::string& line, Tick& tick);

    
    std::string filename_;
    char delimiter_;
    std::ifstream file_;
    size_t line_number_;
    bool is_open_;
};

// ============================================================================
// Binary Tick Reader (for high-performance reading)
// ============================================================================
// Reads ticks in a compact binary format
struct BinaryTick {
    int64_t timestamp;
    char symbol[MAX_SYMBOL_LENGTH];
    int64_t bid_price;
    int64_t ask_price;
    int64_t bid_size;
    int64_t ask_size;
    int64_t last_price;
    int64_t last_size;
    uint64_t volume;
};

// Size depends on MAX_SYMBOL_LENGTH (16) + 9 * 8 bytes = 88 bytes (may have padding)

class BinaryTickReader : public MarketDataFeed {
public:
    explicit BinaryTickReader(const std::string& filename)
        : filename_(filename), is_open_(false), ticks_read_(0), total_ticks_(0) {}
    
    ~BinaryTickReader() override;

    bool open();

    bool next_tick(Tick& tick) override;

    void reset() override;
    
    bool has_data() const override {
        return is_open_ && ticks_read_ < total_ticks_;
    }
    
    size_t total_ticks() const override { return total_ticks_; }

private:
    std::string filename_;
    std::ifstream file_;
    bool is_open_;
    size_t ticks_read_;
    uint64_t total_ticks_;
};

// ============================================================================
// Binary Tick Writer
// ============================================================================
class BinaryTickWriter {
public:
    explicit BinaryTickWriter(const std::string& filename)
        : filename_(filename), ticks_written_(0) {}
    
    ~BinaryTickWriter();

    bool open();

    bool write(const Tick& tick);

    void close();
    
    size_t ticks_written() const { return ticks_written_; }

private:
    std::string filename_;
    std::ofstream file_;
    uint64_t ticks_written_;
};

}  // namespace backtest
